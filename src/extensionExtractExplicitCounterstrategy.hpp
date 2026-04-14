#ifndef __EXTENSION_EXPLICIT_COUNTERSTRATEGY_HPP
#define __EXTENSION_EXPLICIT_COUNTERSTRATEGY_HPP

#include "gr1context.hpp"
#include <string>
#include <unordered_set>
#include <set>
#include <chrono>
#include <iomanip>

/**
 * A class that computes an explicit state counterstrategy for an unrealizable specification
 */
template<class T, bool jsonOutput> class XExtractExplicitCounterStrategy : public T {
protected:
    // New variables
    std::string outputFilename;

    // Inherited stuff used
    using T::mgr;
    using T::strategyDumpingData;
    using T::livenessGuarantees;
    using T::livenessAssumptions;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::varCubePostOutput;
    using T::varCubePostInput;
    using T::varCubePreOutput;
    using T::varCubePreInput;
    using T::varCubePre;
    using T::varCubePost;
    using T::safetyEnv;
    using T::safetySys;
    using T::winningPositions;
    using T::initEnv;
    using T::initSys;
    using T::preVars;
    using T::postVars;
    using T::variableTypes;
    using T::variables;
    using T::variableNames;
    using T::realizable;
    using T::determinize;
    using T::postInputVars;
    using T::postOutputVars;
    using T::doesVariableInheritType;

    XExtractExplicitCounterStrategy<T, jsonOutput>(std::list<std::string> &filenames) : T(filenames) {
        if (filenames.size()==1) {
            outputFilename = "";
        } else {
            outputFilename = filenames.front();
            filenames.pop_front();
        }
    }

public:
    struct ExplicitCounterstrategyStats {
        unsigned int initialStateCount;
        unsigned int stateCount;
        unsigned int transitionCount;
        unsigned int maxOutDegree;
        unsigned int deadlockStateCount;
        unsigned int visitedRankPairCount;
        unsigned int strategyDumpEntries;
    };

    /**
     * @brief Compute and print out (to stdout) an explicit-state counter strategy that is winning for
     *        the environment. The output is compatible with the old JTLV output of LTLMoP.
     *        This function requires that the unrealizability of the specification has already been
     *        detected and that the variables "strategyDumpingData" and
     *        "winningPositions" have been filled by the synthesis algorithm with meaningful data.
     * @param outputStream - Where the strategy shall be printed to.
     */
    void execute() {
            T::execute();
            if (!realizable) {
                auto extractionStart = std::chrono::steady_clock::now();
                ExplicitCounterstrategyStats stats;
                if (outputFilename=="") {
                    stats = computeAndPrintExplicitStateStrategy(std::cout);
                } else {
                    std::ofstream of(outputFilename.c_str());
                    if (of.fail()) {
                        SlugsException ex(false);
                        ex << "Error: Could not open output file'" << outputFilename << "\n";
                        throw ex;
                    }
                    stats = computeAndPrintExplicitStateStrategy(of);
                    if (of.fail()) {
                        SlugsException ex(false);
                        ex << "Error: Writing to output file'" << outputFilename << "failed. \n";
                        throw ex;
                    }
                    of.close();
                }
                auto extractionEnd = std::chrono::steady_clock::now();
                const double extractionSeconds = std::chrono::duration<double>(extractionEnd - extractionStart).count();
                std::cerr << "Timing:\n";
                std::cerr << std::fixed << std::setprecision(6);
                std::cerr << "  - Explicit counterstrategy extraction time: " << extractionSeconds << " s\n";
                const double statesPerSecond = (extractionSeconds > 0.0) ? (static_cast<double>(stats.stateCount) / extractionSeconds) : 0.0;
                const double transitionsPerSecond = (extractionSeconds > 0.0) ? (static_cast<double>(stats.transitionCount) / extractionSeconds) : 0.0;
                const double avgOutDegree = (stats.stateCount > 0) ? (static_cast<double>(stats.transitionCount) / static_cast<double>(stats.stateCount)) : 0.0;
                const unsigned int totalRankPairs = livenessAssumptions.size() * livenessGuarantees.size();
                std::cerr << "Extraction stats:\n";
                std::cerr << "  - Initial explicit states: " << stats.initialStateCount << "\n";
                std::cerr << "  - Explicit states: " << stats.stateCount << "\n";
                std::cerr << "  - Explicit transitions: " << stats.transitionCount << "\n";
                std::cerr << "  - Max out-degree: " << stats.maxOutDegree << "\n";
                std::cerr << "  - Avg out-degree: " << avgOutDegree << "\n";
                std::cerr << "  - Deadlock states: " << stats.deadlockStateCount << "\n";
                std::cerr << "  - Visited rank pairs: " << stats.visitedRankPairCount << " / " << totalRankPairs << "\n";
                std::cerr << "  - Strategy dump entries: " << stats.strategyDumpEntries << "\n";
                std::cerr << "  - Extraction throughput: " << statesPerSecond << " states/s, "
                          << transitionsPerSecond << " transitions/s\n";
            }
    }

    ExplicitCounterstrategyStats computeAndPrintExplicitStateStrategy(std::ostream &outputStream) {

        // We don't want any reordering from this point onwards, as
        // the BDD manipulations from this point onwards are 'kind of simple'.
        mgr.setAutomaticOptimisation(false);

        // List of states in existance so far. The first map
        // maps from a BF node pointer (for the pre variable valuation) and a goal
        // to a state number. The vector then contains the concrete valuation.
        std::map<std::pair<size_t, std::pair<unsigned int, unsigned int> >, unsigned int > lookupTableForPastStates;
        std::vector<BF> bfsUsedInTheLookupTable;
        std::list<std::pair<size_t, std::pair<unsigned int, unsigned int> > > todoList;

        // Deadlock states are synthetic terminal nodes; mark them explicitly so JSON/text stays correct
        std::unordered_set<unsigned int> deadlockStateNums;


        // Prepare positional strategies for the individual goals
        std::vector<std::vector<BF> > positionalStrategiesForTheIndividualGoals(livenessAssumptions.size());
        for (unsigned int i=0;i<livenessAssumptions.size();i++) {
            //BF casesCovered = mgr.constantFalse();
            std::vector<BF> strategy(livenessGuarantees.size()+1);
            for (unsigned int j=0;j<livenessGuarantees.size()+1;j++) {
                strategy[j] = mgr.constantFalse();
            }
            for (auto it = strategyDumpingData.begin();it!=strategyDumpingData.end();it++) {
                if (boost::get<0>(*it) == i) {
                    //Have to cover each guarantee (since the winning strategy depends on which guarantee is being pursued)
                    //Essentially, the choice of which guarantee to pursue can be thought of as a system "move".
                    //The environment always to chooses that prevent the appropriate guarantee.
                    strategy[boost::get<1>(*it)] |= boost::get<2>(*it).UnivAbstract(varCubePostOutput) & !(strategy[boost::get<1>(*it)].ExistAbstract(varCubePost));
                }
            }
            positionalStrategiesForTheIndividualGoals[i] = strategy;
        }

        // Prepare initial to-do list from the allowed initial states. Select a single initial input valuation.

        // TODO: Support for non-special-robotics semantics
        BF todoInit = (winningPositions & initEnv & initSys);
        while (!(todoInit.isFalse())) {
            BF concreteState = determinize(todoInit,preVars);

            //find which liveness guarantee is being prevented (finds the first liveness in order specified)
            // Note by Ruediger here: Removed "!livenessGuarantees[j]" as condition as it is non-positional
            unsigned int found_j_index = 0;
            for (unsigned int j=0;j<livenessGuarantees.size();j++) {
                if (!(concreteState & positionalStrategiesForTheIndividualGoals[0][j]).isFalse()) {
                    found_j_index = j;
                    break;
                }
            }

            std::pair<size_t, std::pair<unsigned int, unsigned int> > lookup = std::pair<size_t, std::pair<unsigned int, unsigned int> >(concreteState.getHashCode(),std::pair<unsigned int, unsigned int>(0,found_j_index));
            lookupTableForPastStates[lookup] = bfsUsedInTheLookupTable.size();
            bfsUsedInTheLookupTable.push_back(concreteState);
            //from now on use the same initial input valuation (but consider all other initial output valuations)
            todoInit &= !concreteState;
            todoList.push_back(lookup);
        }
        const unsigned int initialStateCount = bfsUsedInTheLookupTable.size();

        // Print JSON Header if JSON output is desired
        if (jsonOutput) {
            outputStream << "{\"version\": 0,\n \"slugs\": \"0.0.1\",\n\n";

            // print names of variables
            bool first = true;
            outputStream << " \"variables\": [";
            for (unsigned int i=0; i<variables.size(); i++) {
                if (doesVariableInheritType(i, Pre)) {
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    outputStream << "\"" << variableNames[i] << "\"";
                }
            }
            outputStream << "],\n\n \"nodes\": {\n";
        }

        // Extract strategy
        bool firstNode = true;
        unsigned int transitionCount = 0;
        unsigned int maxOutDegree = 0;
        std::set<std::pair<unsigned int, unsigned int> > visitedRankPairs;
        while (todoList.size()>0) {
            std::pair<size_t, std::pair<unsigned int, unsigned int> > current = todoList.front();
            todoList.pop_front();
            unsigned int stateNum = lookupTableForPastStates[current];
            BF currentPossibilities = bfsUsedInTheLookupTable[stateNum];
            visitedRankPairs.insert(current.second);
            unsigned int stateOutDegree = 0;
            // Print state information
            if (jsonOutput && !firstNode) outputStream << ",\n";
            firstNode = false;
            if (jsonOutput) {
                outputStream << "\"" << stateNum << "\": {\n\t\"rank\": [" << current.second.first << ", " << current.second.second << "],\n\t\"state\": [";
            } else {
                outputStream << "State " << stateNum << " with rank(" << current.second.first << ", " << current.second.second << ") -> <";
            }

            bool first = true;
            bool isDeadlockNode = deadlockStateNums.count(stateNum) != 0;
            for (unsigned int i=0;i<variables.size();i++) {

                if (doesVariableInheritType(i, jsonOutput ? (isDeadlockNode ? PreInput : Pre) : Pre)) {

                    int bit = ((currentPossibilities & variables[i]).isFalse()) ? 0 : 1;

                    if (!first) outputStream << ", ";

                    if (jsonOutput) {
                        outputStream << bit;
                    } else {
                        outputStream << variableNames[i] << ":" << bit;
                    }
                    first = false;
                }
            }

            first = true;

            // If this is a synthetic deadlock node, force it to be terminal.
            // (The normal deadlock predicate is not stable on these nodes.)
            if (deadlockStateNums.count(stateNum)) {
                if (jsonOutput) {
                    outputStream << "],\n\t\"trans\": []\n}";
                   outputStream << "\n\n";
                } else {
                    outputStream << ">\n\tWith no successors.\n";
                }
                if (stateOutDegree > maxOutDegree) {
                    maxOutDegree = stateOutDegree;
                }
                continue;
            }

            // Can we enforce a deadlock?
            BF deadlockInput = (currentPossibilities & safetyEnv & !safetySys).UnivAbstract(varCubePostOutput);
            if (deadlockInput!=mgr.constantFalse()) {

                if (jsonOutput) {
                    outputStream << "],\n";  // end of state list
                    // start list of successors
                    outputStream << "\t\"trans\": [";
                } else {
                    outputStream << "> # Deadlock!\n";
                    outputStream << ">\n\tWith successors : ";
                }
                addDeadlocked(deadlockInput, current, bfsUsedInTheLookupTable,  lookupTableForPastStates,
                              outputStream, todoList, deadlockStateNums);
                transitionCount++;
                stateOutDegree++;

            } else {
                if (jsonOutput) {
                    outputStream << "],\n";  // end of state list
                    // start list of successors
                    outputStream << "\t\"trans\": [";
                } else {
                    outputStream << ">\n\tWith successors : ";
                }
                int succCnt = 0;
                // No deadlock in sight -> Do a normal transition
                BF remainingTransitions = currentPossibilities & positionalStrategiesForTheIndividualGoals[current.second.first][current.second.second];
                assert(remainingTransitions!= mgr.constantFalse());
                remainingTransitions = determinize(remainingTransitions,postInputVars);

                // Switching goals
                while (!(remainingTransitions & safetySys).isFalse()) {

                    BF safeTransition = remainingTransitions & safetySys;
                    BF newCombination = determinize(safeTransition, postOutputVars);

                    // Jump as much forward  in the liveness assumption list as possible ("stuttering avoidance")
                    unsigned int nextLivenessAssumption = current.second.first;
                    bool firstTry = true;
                    while (((nextLivenessAssumption != current.second.first) | firstTry) && !((livenessAssumptions[nextLivenessAssumption] & newCombination).isFalse())) {
                        nextLivenessAssumption  = (nextLivenessAssumption + 1) % livenessAssumptions.size();
                        firstTry = false;
                    }
                    unsigned int nextLivenessGuarantee = current.second.second;
                    firstTry = true;
                    while (((nextLivenessGuarantee != current.second.second) | firstTry) && !((livenessGuarantees[nextLivenessGuarantee] & newCombination).isFalse())) {
                        nextLivenessGuarantee  = (nextLivenessGuarantee + 1) % livenessGuarantees.size();
                        firstTry = false;
                    }

                    //Mark which input has been captured by this case. Use the same input for other successors
                    remainingTransitions &= !newCombination;

                    // We don't need the pre information from the point onwards anymore.
                    newCombination = newCombination.ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);

                    unsigned int tn;

                    std::pair<size_t, std::pair<unsigned int, unsigned int> > target;

                    target = std::pair<size_t, std::pair<unsigned int, unsigned int> >(newCombination.getHashCode(),std::pair<unsigned int, unsigned int>(nextLivenessAssumption, nextLivenessGuarantee));

                    if (lookupTableForPastStates.count(target)==0) {
                        tn = lookupTableForPastStates[target] = bfsUsedInTheLookupTable.size();
                        bfsUsedInTheLookupTable.push_back(newCombination);
                        todoList.push_back(target);
                    } else {
                        tn = lookupTableForPastStates[target];
                    }

                    // Print
                    if (first) {
                        first = false;
                    } else {
                        outputStream << ", ";
                    }
                    outputStream << tn;
                    succCnt++;
                    transitionCount++;
                    stateOutDegree++;
                    if (succCnt > 200) {
                        if (!jsonOutput) {
                            outputStream << "\n";
                            outputStream << "\t# Early break after 200 successors!\n";
                        }
                        break;
                    }
                }
            }
            if (jsonOutput) {
                outputStream << "]\n}";
                outputStream << "\n\n";
            } else {
                outputStream << "\n";
            }
            if (stateOutDegree > maxOutDegree) {
                maxOutDegree = stateOutDegree;
            }
        }
        if (jsonOutput) {
            // close "nodes" dict and json object
            outputStream << "}}\n";
        }

        ExplicitCounterstrategyStats stats;
        stats.initialStateCount = initialStateCount;
        stats.stateCount = bfsUsedInTheLookupTable.size();
        stats.transitionCount = transitionCount;
        stats.maxOutDegree = maxOutDegree;
        stats.deadlockStateCount = deadlockStateNums.size();
        stats.visitedRankPairCount = visitedRankPairs.size();
        stats.strategyDumpEntries = strategyDumpingData.size();
        return stats;
    }

    //This function adds a new successor-less "state" that captures the deadlock-causing input values
    //The outputvalues are omitted (indeed, no valuation exists that satisfies the system safeties)
    //Format compatible with JTLV counterstrategy

    void addDeadlocked(BF targetPositionCandidateSet,
        std::pair<size_t, std::pair<unsigned int, unsigned int> > current,
        std::vector<BF> &bfsUsedInTheLookupTable,
        std::map<std::pair<size_t, std::pair<unsigned int, unsigned int> >, unsigned int > &lookupTableForPastStates,
        std::ostream &outputStream,
        std::list<std::pair<size_t, std::pair<unsigned int, unsigned int> > > &todoList,
        std::unordered_set<unsigned int> &deadlockStateNums
    ) {

        BF newCombination = determinize(targetPositionCandidateSet, postVars);

        newCombination =
            (newCombination.ExistAbstract(varCubePostOutput).ExistAbstract(varCubePre))
                .SwapVariables(varVectorPre, varVectorPost);

        std::pair<size_t, std::pair<unsigned int, unsigned int> > target(
            newCombination.getHashCode(),
            std::pair<unsigned int, unsigned int>(current.second.first, current.second.second)
        );

        unsigned int tn;
        bool isNew = false;

        if (lookupTableForPastStates.count(target) == 0) {
            tn = lookupTableForPastStates[target] = bfsUsedInTheLookupTable.size();
            bfsUsedInTheLookupTable.push_back(newCombination);
            isNew = true;
        } else {
            tn = lookupTableForPastStates[target];
        }

        // In JSON mode, ONLY print the successor node id (the caller is inside "trans":[ ... ]).
        if (jsonOutput) {
            outputStream << tn;
            if (isNew) {
                todoList.push_back(target);          // ensure the node is emitted later
            }
            deadlockStateNums.insert(tn);            // ensure it prints as terminal (trans: [])
            return;
        }

        // Non-JSON mode: preserve existing verbose behavior
        if (isNew) {
            outputStream << tn << "\n";

            // Note: printing here can cause out-of-order state printing, as your comment says.
            outputStream << "State " << tn << " with rank (" << current.second.first
                        << "," << current.second.second << ") -> <";

            bool first = true;
            for (unsigned int i = 0; i < variables.size(); i++) {
                if (doesVariableInheritType(i, PreInput)) {
                    if (!first) outputStream << ", ";
                    first = false;
                    outputStream << variableNames[i] << ":"
                                << (((newCombination & variables[i]).isFalse()) ? 0 : 1);
                }
            }
            outputStream << ">\n\tWith no successors.";
        } else {
            outputStream << tn;
        }
    }


    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XExtractExplicitCounterStrategy<T, jsonOutput>(filenames);
    }
};

#endif
