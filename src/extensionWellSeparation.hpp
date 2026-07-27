#ifndef __EXTENSION_WELL_SEPARATION_HPP
#define __EXTENSION_WELL_SEPARATION_HPP

#include "gr1context.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

/**
 * Experimental well-separation diagnostics for GR(1) specifications.
 *
 * Implements the Maoz/Ringert case diagnosis by reducing the question to
 * Slugs' existing GR(1) system-winning-state computation with the system
 * specification <true,true,{false}>.
 */
template<class T, bool writeJsonToFile, bool minimizeCore> class XWellSeparation : public T {
protected:
    using T::initEnv;
    using T::initSys;
    using T::lineNumberCurrentlyRead;
    using T::livenessAssumptions;
    using T::livenessGuarantees;
    using T::mgr;
    using T::addVariable;
    using T::parseBooleanFormula;
    using T::preVars;
    using T::safetyEnv;
    using T::safetyEnvFormulae;
    using T::safetySys;
    using T::safetySysFormulae;
    using T::varCubePre;
    using T::varCubePreInput;
    using T::varCubePreOutput;
    using T::varCubePostInput;
    using T::varVectorPre;
    using T::varVectorPost;
    using T::variables;
    using T::variableNames;
    using T::variableTypes;
    using T::winningPositions;

    struct NamedAssumption {
        std::string kind;
        unsigned int index;
        unsigned int sourceIndex;
        unsigned int line;
        std::string name;
    };

    struct StateWitness {
        bool available;
        std::string type;
        std::string sourceRegion;
        std::string wellSeparationCase;
        std::string unavailableReason;
        std::string detailsKind;
        bool allNextInputsViolateSafety;
        std::vector<NamedAssumption> implicatedAssumptions;
        std::vector<NamedAssumption> detailAssumptions;
        BF cube;

        StateWitness(BF falseValue) :
            available(false),
            type("state_cube"),
            sourceRegion("none"),
            wellSeparationCase("none"),
            unavailableReason("none"),
            detailsKind("none"),
            allNextInputsViolateSafety(false),
            cube(falseValue) {}
    };

    struct CoreCandidate {
        std::string kind;
        unsigned int index;
    };

    struct CoreResult {
        bool enabled;
        bool complete;
        std::string mode;
        unsigned int candidateCount;
        unsigned int solverCalls;
        std::vector<NamedAssumption> assumptions;

        CoreResult() :
            enabled(false),
            complete(false),
            mode("disabled"),
            candidateCount(0),
            solverCalls(0) {}
    };

    class TemporaryWellSeparationGame {
    private:
        XWellSeparation<T,writeJsonToFile,minimizeCore> &context;
        std::vector<BF> oldLivenessAssumptions;
        std::vector<BF> oldLivenessGuarantees;
        BF oldSafetySys;
    public:
        TemporaryWellSeparationGame(XWellSeparation<T,writeJsonToFile,minimizeCore> &_context,bool includeEnvironmentLiveness) :
            context(_context),
            oldLivenessAssumptions(_context.livenessAssumptions),
            oldLivenessGuarantees(_context.livenessGuarantees),
            oldSafetySys(_context.safetySys) {

            context.safetySys = context.mgr.constantTrue();
            context.livenessGuarantees.clear();
            context.livenessGuarantees.push_back(context.mgr.constantFalse());
            if (!includeEnvironmentLiveness) {
                context.livenessAssumptions.clear();
                context.livenessAssumptions.push_back(context.mgr.constantTrue());
            }
        }

        ~TemporaryWellSeparationGame() {
            context.safetySys = oldSafetySys;
            context.livenessGuarantees = oldLivenessGuarantees;
            context.livenessAssumptions = oldLivenessAssumptions;
        }
    };

    std::string jsonOutputFilename;
    std::vector<BF> envInitFormulae;
    std::vector<std::string> envInitNames;
    std::vector<unsigned int> envInitLines;
    std::vector<std::string> envSafetyNames;
    std::vector<unsigned int> envSafetyLines;
    std::vector<std::string> envLivenessNames;
    std::vector<unsigned int> envLivenessLines;

    XWellSeparation(std::list<std::string> &filenames) : T(filenames) {}

    static std::string jsonString(const std::string &value) {
        std::ostringstream out;
        out << '"';
        for (unsigned int i=0;i<value.size();i++) {
            char c = value[i];
            if (c=='"' || c=='\\') {
                out << '\\' << c;
            } else if (c=='\n') {
                out << "\\n";
            } else if (c=='\r') {
                out << "\\r";
            } else if (c=='\t') {
                out << "\\t";
            } else {
                out << c;
            }
        }
        out << '"';
        return out.str();
    }

    static const char *jsonBool(bool value) {
        return value ? "true" : "false";
    }

    static double secondsBetween(std::chrono::steady_clock::time_point start,std::chrono::steady_clock::time_point end) {
        return std::chrono::duration<double>(end - start).count();
    }

    static std::string makeDefaultAssumptionName(const std::string &prefix, unsigned int index) {
        std::ostringstream name;
        name << prefix << " " << index;
        return name.str();
    }

    static std::string consumeAssumptionName(std::string &currentPropertyName,const std::string &prefix,unsigned int index) {
        if (currentPropertyName.length()>0) {
            std::string result = currentPropertyName;
            currentPropertyName = "";
            return result;
        }
        return makeDefaultAssumptionName(prefix,index);
    }

    NamedAssumption makeNamedAssumption(const std::string &kind,unsigned int index,const std::vector<std::string> &names,const std::vector<unsigned int> &lines) {
        NamedAssumption assumption;
        assumption.kind = kind;
        assumption.index = index;
        assumption.sourceIndex = index;
        assumption.line = lines[index];
        assumption.name = names[index];
        return assumption;
    }

    NamedAssumption makeNamedAssumption(const CoreCandidate &candidate) {
        if (candidate.kind=="env_init") return makeNamedAssumption("env_init",candidate.index,envInitNames,envInitLines);
        if (candidate.kind=="env_safety") return makeNamedAssumption("env_safety",candidate.index,envSafetyNames,envSafetyLines);
        return makeNamedAssumption("env_liveness",candidate.index,envLivenessNames,envLivenessLines);
    }

    void appendNamedAssumptions(std::vector<NamedAssumption> &target,const std::string &kind,const std::vector<std::string> &names,const std::vector<unsigned int> &lines) {
        for (unsigned int i=0;i<names.size();i++) {
            target.push_back(makeNamedAssumption(kind,i,names,lines));
        }
    }

    void appendResponsibleAssumptionsForCases(std::vector<NamedAssumption> &target,const std::vector<std::string> &cases) {
        bool includeInit = false;
        bool includeSafety = false;
        bool includeLiveness = false;
        for (unsigned int i=0;i<cases.size();i++) {
            includeInit = includeInit || (cases[i].find("E-ini")!=std::string::npos);
            includeSafety = includeSafety || (cases[i].find("E-safe")!=std::string::npos);
            includeLiveness = includeLiveness || (cases[i].find("E-just")!=std::string::npos);
        }
        if (includeInit) appendNamedAssumptions(target,"env_init",envInitNames,envInitLines);
        if (includeSafety) appendNamedAssumptions(target,"env_safety",envSafetyNames,envSafetyLines);
        if (includeLiveness) appendNamedAssumptions(target,"env_liveness",envLivenessNames,envLivenessLines);
    }

    StateWitness makeStateWitness(BF region,const std::string &sourceRegion,const std::string &wellSeparationCase) {
        StateWitness witness(mgr.constantFalse());
        if (region.isFalse()) {
            return witness;
        }

        witness.available = true;
        witness.sourceRegion = sourceRegion;
        witness.wellSeparationCase = wellSeparationCase;
        witness.cube = T::determinize(region,preVars);
        return witness;
    }

    void appendSafetyWitnessImplications(StateWitness &witness) {
        if (!witness.available) return;
        for (unsigned int i=0;i<safetyEnvFormulae.size();i++) {
            if (!(witness.cube & !safetyEnvFormulae[i]).isFalse()) {
                witness.implicatedAssumptions.push_back(makeNamedAssumption("env_safety",i,envSafetyNames,envSafetyLines));
            }
        }
        witness.detailAssumptions = witness.implicatedAssumptions;
    }

    void appendLivenessWitnessImplications(StateWitness &witness) {
        if (!witness.available) return;
        for (unsigned int i=0;i<livenessAssumptions.size();i++) {
            if (!(witness.cube & !livenessAssumptions[i]).isFalse()) {
                witness.implicatedAssumptions.push_back(makeNamedAssumption("env_liveness",i,envLivenessNames,envLivenessLines));
            }
        }
        witness.detailAssumptions = witness.implicatedAssumptions;
    }

    void writeWitnessValuationObject(std::ostringstream &json,BF cube,VariableType variableType) {
        bool first = true;
        for (unsigned int i=0;i<variableNames.size();i++) {
            if (variableTypes[i]!=variableType) continue;

            if (!first) json << ", ";
            first = false;

            bool value = !(cube & variables[i]).isFalse();
            json << jsonString(variableNames[i]) << ": " << jsonBool(value);
        }
    }

    void writeNamedAssumptionArray(std::ostringstream &json,const std::vector<NamedAssumption> &assumptions) {
        json << "[";
        for (unsigned int i=0;i<assumptions.size();i++) {
            if (i>0) json << ", ";
            json << "{";
            json << "\"kind\": " << jsonString(assumptions[i].kind) << ", ";
            json << "\"index\": " << assumptions[i].index << ", ";
            json << "\"source_index\": " << assumptions[i].sourceIndex << ", ";
            json << "\"line\": " << assumptions[i].line << ", ";
            json << "\"name\": " << jsonString(assumptions[i].name);
            json << "}";
        }
        json << "]";
    }

    unsigned int countVariablesOfType(VariableType variableType) {
        unsigned int count = 0;
        for (unsigned int i=0;i<variableTypes.size();i++) {
            if (variableTypes[i]==variableType) count++;
        }
        return count;
    }

    void writeWitnessJson(std::ostringstream &json,const StateWitness &witness) {
        json << "  \"witness_available\": " << jsonBool(witness.available) << ",\n";
        json << "  \"witness_unavailable_reason\": " << jsonString(witness.available ? "none" : witness.unavailableReason) << ",\n";
        json << "  \"witness\": ";
        if (!witness.available) {
            json << "null,\n";
            return;
        }

        json << "{\n";
        json << "    \"type\": " << jsonString(witness.type) << ",\n";
        json << "    \"source_region\": " << jsonString(witness.sourceRegion) << ",\n";
        json << "    \"case\": " << jsonString(witness.wellSeparationCase) << ",\n";
        json << "    \"inputs\": {";
        writeWitnessValuationObject(json,witness.cube,PreInput);
        json << "},\n";
        json << "    \"outputs\": {";
        writeWitnessValuationObject(json,witness.cube,PreOutput);
        json << "},\n";
        json << "    \"implicated_assumptions\": [";
        for (unsigned int i=0;i<witness.implicatedAssumptions.size();i++) {
            if (i>0) json << ", ";
            json << "{";
            json << "\"kind\": " << jsonString(witness.implicatedAssumptions[i].kind) << ", ";
            json << "\"index\": " << witness.implicatedAssumptions[i].index << ", ";
            json << "\"source_index\": " << witness.implicatedAssumptions[i].sourceIndex << ", ";
            json << "\"line\": " << witness.implicatedAssumptions[i].line << ", ";
            json << "\"name\": " << jsonString(witness.implicatedAssumptions[i].name);
            json << "}";
        }
        json << "],\n";
        json << "    \"details\": {\n";
        json << "      \"kind\": " << jsonString(witness.detailsKind) << ",\n";
        json << "      \"all_next_inputs_violate_env_safety\": " << jsonBool(witness.allNextInputsViolateSafety) << ",\n";
        json << "      \"next_input_variable_count\": " << countVariablesOfType(PostInput) << ",\n";
        json << "      \"strategy_region\": " << jsonString(witness.sourceRegion) << ",\n";
        json << "      \"assumptions\": ";
        writeNamedAssumptionArray(json,witness.detailAssumptions);
        json << "\n";
        json << "    }\n";
        json << "  },\n";
    }

    void writeCoreJson(std::ostringstream &json,const CoreResult &core) {
        json << "  \"core_mode\": " << jsonString(core.mode) << ",\n";
        json << "  \"core_complete\": " << jsonBool(core.complete) << ",\n";
        json << "  \"core_enabled\": " << jsonBool(core.enabled) << ",\n";
        json << "  \"core_candidate_count\": " << core.candidateCount << ",\n";
        json << "  \"core_solver_calls\": " << core.solverCalls << ",\n";
        json << "  \"core_assumptions\": ";
        writeNamedAssumptionArray(json,core.assumptions);
        json << ",\n";
    }

    BF computeEnvironmentReachableStates() {
        BFFixedPoint muReach(initEnv);
        for (;!muReach.isFixedPointReached();) {
            BF nextStates = (muReach.getValue() & safetyEnv).ExistAbstract(varCubePre).SwapVariables(varVectorPre,varVectorPost);
            muReach.update(muReach.getValue() | nextStates);
        }
        return muReach.getValue();
    }

    BF computeDirectEnvironmentSafetyViolationStates() {
        return (!safetyEnv).UnivAbstract(varCubePostInput);
    }

    BF computeEnvironmentViolationWinningStates(bool includeEnvironmentLiveness) {
        TemporaryWellSeparationGame temporaryGame(*this,includeEnvironmentLiveness);
        T::computeWinningPositions();
        return winningPositions;
    }

    // Maoz/Ringert Def. 3 (non-well-separated core) intersects the candidate
    // subset's winning states with reachStates of the *original* assumption
    // set, not the candidate's own reachable states. Removing assumptions can
    // only enlarge the reachable region, so recomputing reachability from a
    // reduced candidate would let the minimization search "discover" violations
    // through states that are not actually reachable in the original
    // specification, misattributing the core. originalReachableStates must
    // therefore stay fixed across the whole search.
    bool currentEnvironmentIsNonWellSeparated(BF originalReachableStates) {
        if (initEnv.isFalse()) return true;

        BF safetyWinningStates = computeEnvironmentViolationWinningStates(false);
        if (!(safetyWinningStates & originalReachableStates).isFalse()) return true;

        BF fullWinningStates = computeEnvironmentViolationWinningStates(true);
        return !(fullWinningStates & originalReachableStates).isFalse();
    }

    void applyEnvironmentCoreMask(const std::vector<BF> &originalEnvInitFormulae,
                                  const std::vector<BF> &originalSafetyEnvFormulae,
                                  const std::vector<BF> &originalLivenessAssumptions,
                                  const std::vector<bool> &keepInit,
                                  const std::vector<bool> &keepSafety,
                                  const std::vector<bool> &keepLiveness) {
        initEnv = mgr.constantTrue();
        for (unsigned int i=0;i<originalEnvInitFormulae.size();i++) {
            if (keepInit[i]) initEnv &= originalEnvInitFormulae[i];
        }

        safetyEnv = mgr.constantTrue();
        safetyEnvFormulae.clear();
        for (unsigned int i=0;i<originalSafetyEnvFormulae.size();i++) {
            if (keepSafety[i]) {
                safetyEnvFormulae.push_back(originalSafetyEnvFormulae[i]);
                safetyEnv &= originalSafetyEnvFormulae[i];
            }
        }

        livenessAssumptions.clear();
        for (unsigned int i=0;i<originalLivenessAssumptions.size();i++) {
            if (keepLiveness[i]) livenessAssumptions.push_back(originalLivenessAssumptions[i]);
        }
        if (livenessAssumptions.size()==0) {
            livenessAssumptions.push_back(mgr.constantTrue());
        }
    }

    // Applies the environment corresponding to keeping exactly the candidates
    // at 'keptCandidateIndices[i]==true' (indices into 'candidates'), plus any
    // synthetic default-true liveness assumption (envLivenessLines[i]==0, added
    // when the spec has no [ENV_LIVENESS] section at all), which is never a
    // core candidate and always stays kept.
    void applyKeptCandidateSet(const std::vector<CoreCandidate> &candidates,
                               const std::vector<BF> &originalEnvInitFormulae,
                               const std::vector<BF> &originalSafetyEnvFormulae,
                               const std::vector<BF> &originalLivenessAssumptions,
                               const std::vector<bool> &keptCandidateIndices) {
        std::vector<bool> keepInit(originalEnvInitFormulae.size(),false);
        std::vector<bool> keepSafety(originalSafetyEnvFormulae.size(),false);
        std::vector<bool> keepLiveness(originalLivenessAssumptions.size(),false);
        for (unsigned int i=0;i<candidates.size();i++) {
            if (!keptCandidateIndices[i]) continue;
            const CoreCandidate &candidate = candidates[i];
            if (candidate.kind=="env_init") keepInit[candidate.index] = true;
            else if (candidate.kind=="env_safety") keepSafety[candidate.index] = true;
            else keepLiveness[candidate.index] = true;
        }
        for (unsigned int i=0;i<originalLivenessAssumptions.size();i++) {
            if (i<envLivenessLines.size() && envLivenessLines[i]==0) keepLiveness[i] = true;
        }
        applyEnvironmentCoreMask(originalEnvInitFormulae,originalSafetyEnvFormulae,originalLivenessAssumptions,keepInit,keepSafety,keepLiveness);
    }

    // Delta debugging (Zeller's ddmin), matching the search strategy of the
    // reference implementations this mode is validated against: Maoz/Ringert
    // FSE 2016 Sect. 5.3 ("we implemented the computation of non-well-separated
    // cores ... using the delta-debugging algorithm of Zeller") and
    // Gorenstein/Maoz/Ringert ICSE 2024 Sect. 6.3 ("we utilize Delta Debugging
    // (DDMin) to compute a core"). Repeatedly tests ever-finer partitions of
    // the current candidate set and their complements, instead of a single
    // linear deletion pass, which needs fewer solver calls than a plain greedy
    // scan whenever the eventual core is small relative to the candidate count
    // (the common case per both papers' evaluations). Like a greedy scan, this
    // only guarantees a *1-minimal* core (no single candidate removable), not a
    // global minimum; which 1-minimal core is found can depend on candidate
    // order, same as a greedy scan.
    //
    // Correctness relies on the same monotonicity as the linear scan did: with
    // originalReachableStates pinned (see currentEnvironmentIsNonWellSeparated),
    // "is this candidate subset non-well-separated" is monotonically increasing
    // in subset inclusion (Maoz/Ringert Theorem 2, "Core Monotonic"), so ddmin's
    // subset/complement tests are searching a well-founded upward-closed
    // predicate exactly as ddmin's design assumes.
    std::vector<unsigned int> ddminSearch(unsigned int candidateCount,
                                          const std::function<bool(const std::vector<unsigned int>&)> &testSubset) {
        std::vector<unsigned int> workingSet;
        for (unsigned int i=0;i<candidateCount;i++) workingSet.push_back(i);

        unsigned int granularity = 2;
        while (workingSet.size()>=2) {
            unsigned int n = std::min<unsigned int>(granularity,(unsigned int)workingSet.size());
            std::vector<std::vector<unsigned int> > chunks(n);
            for (unsigned int i=0;i<workingSet.size();i++) {
                chunks[i*n/workingSet.size()].push_back(workingSet[i]);
            }

            bool reduced = false;
            for (unsigned int c=0;c<n && !reduced;c++) {
                if (chunks[c].empty()) continue;
                if (testSubset(chunks[c])) {
                    workingSet = chunks[c];
                    granularity = std::max<unsigned int>(granularity-1,2);
                    reduced = true;
                }
            }
            if (!reduced) {
                for (unsigned int c=0;c<n && !reduced;c++) {
                    if (chunks[c].empty()) continue;
                    std::vector<unsigned int> complement;
                    for (unsigned int cc=0;cc<n;cc++) {
                        if (cc==c) continue;
                        complement.insert(complement.end(),chunks[cc].begin(),chunks[cc].end());
                    }
                    if (complement.empty()) continue;
                    if (testSubset(complement)) {
                        workingSet = complement;
                        granularity = std::max<unsigned int>(granularity-1,2);
                        reduced = true;
                    }
                }
            }
            if (!reduced) {
                if (granularity>=workingSet.size()) break;
                granularity = std::min<unsigned int>(granularity*2,(unsigned int)workingSet.size());
            }
        }
        return workingSet;
    }

    CoreResult computeDeltaDebuggingCoreIfRequested(bool originalNonWellSeparated) {
        CoreResult result;
        result.enabled = minimizeCore;
        if (!minimizeCore) return result;

        result.mode = "delta_debugging_1_minimal_non_well_separated_subset";
        if (!originalNonWellSeparated) {
            result.complete = true;
            return result;
        }

        BF originalReachableStates = computeEnvironmentReachableStates();

        std::vector<BF> originalEnvInitFormulae = envInitFormulae;
        std::vector<BF> originalSafetyEnvFormulae = safetyEnvFormulae;
        std::vector<BF> originalLivenessAssumptions = livenessAssumptions;
        BF originalInitEnv = initEnv;
        BF originalSafetyEnv = safetyEnv;

        std::vector<CoreCandidate> candidates;
        for (unsigned int i=0;i<originalEnvInitFormulae.size();i++) {
            CoreCandidate candidate;
            candidate.kind = "env_init";
            candidate.index = i;
            candidates.push_back(candidate);
        }
        for (unsigned int i=0;i<originalSafetyEnvFormulae.size();i++) {
            CoreCandidate candidate;
            candidate.kind = "env_safety";
            candidate.index = i;
            candidates.push_back(candidate);
        }
        for (unsigned int i=0;i<originalLivenessAssumptions.size();i++) {
            if (i<envLivenessLines.size() && envLivenessLines[i]==0) continue;
            CoreCandidate candidate;
            candidate.kind = "env_liveness";
            candidate.index = i;
            candidates.push_back(candidate);
        }
        result.candidateCount = candidates.size();

        std::vector<unsigned int> core = ddminSearch(candidates.size(),
            [&](const std::vector<unsigned int> &subsetIndices) -> bool {
                std::vector<bool> kept(candidates.size(),false);
                for (unsigned int idx : subsetIndices) kept[idx] = true;
                applyKeptCandidateSet(candidates,originalEnvInitFormulae,originalSafetyEnvFormulae,originalLivenessAssumptions,kept);
                result.solverCalls++;
                return currentEnvironmentIsNonWellSeparated(originalReachableStates);
            });

        for (unsigned int idx : core) {
            result.assumptions.push_back(makeNamedAssumption(candidates[idx]));
        }

        initEnv = originalInitEnv;
        safetyEnv = originalSafetyEnv;
        safetyEnvFormulae = originalSafetyEnvFormulae;
        livenessAssumptions = originalLivenessAssumptions;
        result.complete = true;
        return result;
    }

    bool allEnvironmentInitialInputsCanBeWon(BF winningStates) {
        BF result = initEnv.Implies(winningStates.ExistAbstract(varCubePreOutput)).UnivAbstract(varCubePreInput);
        if (!result.isConstant()) {
            throw "Internal error: Could not establish well-separation initial-state case.";
        }
        return result.isTrue();
    }

public:
    void init(std::list<std::string> &filenames) {
        if (filenames.size()==0) {
            throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
        }

        std::string inFileName = filenames.front();
        filenames.pop_front();

        std::ifstream inFile(inFileName.c_str());
        if (inFile.fail()) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot open input file '" << inFileName << "'";
            throw errorMessage.str();
        }

        initEnv = mgr.constantTrue();
        initSys = mgr.constantTrue();
        safetyEnv = mgr.constantTrue();
        safetySys = mgr.constantTrue();

        int readMode = -1;
        std::string currentLine;
        std::string currentPropertyName = "";
        lineNumberCurrentlyRead = 0;
        while (std::getline(inFile,currentLine)) {
            lineNumberCurrentlyRead++;
            boost::trim(currentLine);
            if (currentLine.substr(0,2)=="##") {
                currentPropertyName = currentLine.substr(2,std::string::npos);
                boost::trim(currentPropertyName);
            } else if ((currentLine.length()>0) && (currentLine[0]!='#')) {
                if (currentLine[0]=='[') {
                    if (currentLine=="[INPUT]") {
                        readMode = 0;
                    } else if (currentLine=="[OUTPUT]") {
                        readMode = 1;
                    } else if (currentLine=="[ENV_INIT]") {
                        readMode = 2;
                    } else if (currentLine=="[SYS_INIT]") {
                        readMode = 3;
                    } else if (currentLine=="[ENV_TRANS]") {
                        readMode = 4;
                    } else if (currentLine=="[SYS_TRANS]") {
                        readMode = 5;
                    } else if (currentLine=="[ENV_LIVENESS]") {
                        readMode = 6;
                    } else if (currentLine=="[SYS_LIVENESS]") {
                        readMode = 7;
                    } else {
                        std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                        throw "Aborted.";
                    }
                } else {
                    if (readMode==0) {
                        addVariable(PreInput,currentLine);
                        addVariable(PostInput,currentLine+"'");
                    } else if (readMode==1) {
                        addVariable(PreOutput,currentLine);
                        addVariable(PostOutput,currentLine+"'");
                    } else if (readMode==2) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        BF bf = parseBooleanFormula(currentLine,allowedTypes);
                        initEnv &= bf;
                        envInitFormulae.push_back(bf);
                        envInitNames.push_back(consumeAssumptionName(currentPropertyName,"Initial Assumption",envInitFormulae.size()));
                        envInitLines.push_back(lineNumberCurrentlyRead);
                    } else if (readMode==3) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        initSys &= parseBooleanFormula(currentLine,allowedTypes);
                    } else if (readMode==4) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        BF bf = parseBooleanFormula(currentLine,allowedTypes);
                        safetyEnvFormulae.push_back(bf);
                        safetyEnv &= bf;
                        envSafetyNames.push_back(consumeAssumptionName(currentPropertyName,"Safety Assumption",safetyEnvFormulae.size()));
                        envSafetyLines.push_back(lineNumberCurrentlyRead);
                    } else if (readMode==5) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        BF bf = parseBooleanFormula(currentLine,allowedTypes);
                        safetySysFormulae.push_back(bf);
                        safetySys &= bf;
                        if (safetySys.isFalse()) {
                            std::cerr << " safetySys is false at '" << currentLine << "'" << std::endl;
                        }
                    } else if (readMode==6) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostOutput);
                        allowedTypes.insert(PostInput);
                        livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                        envLivenessNames.push_back(consumeAssumptionName(currentPropertyName,"Liveness Assumption",livenessAssumptions.size()));
                        envLivenessLines.push_back(lineNumberCurrentlyRead);
                    } else if (readMode==7) {
                        std::set<VariableType> allowedTypes;
                        allowedTypes.insert(PreInput);
                        allowedTypes.insert(PreOutput);
                        allowedTypes.insert(PostInput);
                        allowedTypes.insert(PostOutput);
                        livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                    } else {
                        std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                        throw "Found a line in the specification file that has no proper categorial context.";
                    }
                    currentPropertyName = "";
                }
            }
        }

        std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
        if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

        if (livenessAssumptions.size()==0) {
            livenessAssumptions.push_back(mgr.constantTrue());
            envLivenessNames.push_back("Automatically added TRUE liveness assumption");
            envLivenessLines.push_back(0);
        }
        if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());

        if (writeJsonToFile) {
            if (filenames.size()==0) {
                throw SlugsException(false,"Error: --checkWellSeparation --jsonOutput requires an output JSON filename.");
            }
            jsonOutputFilename = filenames.front();
            filenames.pop_front();
        }
    }

    void execute() {
        auto analysisStart = std::chrono::steady_clock::now();

        std::vector<std::string> warnings;
        warnings.push_back("Auxiliary-variable pattern semantics are not separated from environment assumptions in this Slugs-level mode.");

        bool envInitContradiction = initEnv.isFalse();

        std::string status = "ANALYSIS_INCOMPLETE";
        std::string violatedAssumptionType = "none";
        bool complete = true;
        std::vector<std::string> cases;

        BF reachableStates = mgr.constantFalse();
        BF safetyWinningStates = mgr.constantFalse();
        BF directSafetyViolationStates = mgr.constantFalse();
        BF fullWinningStates = mgr.constantFalse();
        bool safetyReachableWin = false;
        bool safetyAllInitialWin = false;
        bool livenessChecked = false;
        bool livenessReachableWin = false;
        bool livenessAllInitialWin = false;
        std::vector<NamedAssumption> responsibleAssumptions;
        StateWitness witness(mgr.constantFalse());
        double reachabilitySeconds = 0.0;
        double safetyWinningSeconds = 0.0;
        double directSafetySeconds = 0.0;
        double livenessWinningSeconds = 0.0;
        double diagnosticsSeconds = 0.0;
        double coreSeconds = 0.0;

        if (envInitContradiction) {
            status = "NON_WELL_SEPARATED";
            violatedAssumptionType = "env_init";
            cases.push_back("P-all/E-ini");
            witness.unavailableReason = "unsat_env_init";
        } else {
            auto reachabilityStart = std::chrono::steady_clock::now();
            reachableStates = computeEnvironmentReachableStates();
            auto reachabilityEnd = std::chrono::steady_clock::now();
            reachabilitySeconds = secondsBetween(reachabilityStart,reachabilityEnd);

            auto safetyWinningStart = std::chrono::steady_clock::now();
            safetyWinningStates = computeEnvironmentViolationWinningStates(false);
            auto safetyWinningEnd = std::chrono::steady_clock::now();
            safetyWinningSeconds = secondsBetween(safetyWinningStart,safetyWinningEnd);

            safetyReachableWin = !(safetyWinningStates & reachableStates).isFalse();
            if (safetyReachableWin) {
                safetyAllInitialWin = allEnvironmentInitialInputsCanBeWon(safetyWinningStates);
                auto directSafetyStart = std::chrono::steady_clock::now();
                directSafetyViolationStates = computeDirectEnvironmentSafetyViolationStates();
                auto directSafetyEnd = std::chrono::steady_clock::now();
                directSafetySeconds = secondsBetween(directSafetyStart,directSafetyEnd);
                if (safetyAllInitialWin) {
                    status = "NON_WELL_SEPARATED";
                    violatedAssumptionType = "env_safety";
                    cases.push_back("P-all/E-safe");
                } else {
                    cases.push_back("P-reach/E-safe");
                }
            }

            if (cases.size()==0 || !safetyAllInitialWin) {
                livenessChecked = true;
                auto livenessWinningStart = std::chrono::steady_clock::now();
                fullWinningStates = computeEnvironmentViolationWinningStates(true);
                auto livenessWinningEnd = std::chrono::steady_clock::now();
                livenessWinningSeconds = secondsBetween(livenessWinningStart,livenessWinningEnd);
                livenessReachableWin = !(fullWinningStates & reachableStates).isFalse();
                if (livenessReachableWin) {
                    livenessAllInitialWin = allEnvironmentInitialInputsCanBeWon(fullWinningStates);
                    if (livenessAllInitialWin) {
                        cases.push_back("P-all/E-just");
                    } else if (cases.size()==0) {
                        cases.push_back("P-reach/E-just");
                    }
                }
            }

            if (cases.size()==0) {
                status = "WELL_SEPARATED";
                witness.unavailableReason = "well_separated";
            } else {
                status = "NON_WELL_SEPARATED";
                if (safetyReachableWin) {
                    violatedAssumptionType = "env_safety";
                } else {
                    violatedAssumptionType = "env_liveness";
                }
            }
        }

        auto diagnosticsStart = std::chrono::steady_clock::now();
        appendResponsibleAssumptionsForCases(responsibleAssumptions,cases);
        if (safetyReachableWin) {
            std::string witnessCase = safetyAllInitialWin ? "P-all/E-safe" : "P-reach/E-safe";
            BF directWitnessRegion = directSafetyViolationStates & reachableStates;
            if (!directWitnessRegion.isFalse()) {
                witness = makeStateWitness(directWitnessRegion,"direct_safety_violation_reachable",witnessCase);
                witness.detailsKind = "one_step_safety_violation";
                witness.allNextInputsViolateSafety = true;
            } else {
                witness = makeStateWitness(safetyWinningStates & reachableStates,"safety_winning_reachable",witnessCase);
                witness.detailsKind = "safety_winning_region";
            }
            appendSafetyWitnessImplications(witness);
        } else if (livenessReachableWin) {
            std::string witnessCase = livenessAllInitialWin ? "P-all/E-just" : "P-reach/E-just";
            witness = makeStateWitness(fullWinningStates & reachableStates,"full_winning_reachable",witnessCase);
            witness.detailsKind = "abstract_liveness_trap";
            appendLivenessWitnessImplications(witness);
        }
        auto diagnosticsEnd = std::chrono::steady_clock::now();
        diagnosticsSeconds = secondsBetween(diagnosticsStart,diagnosticsEnd);

        auto coreStart = std::chrono::steady_clock::now();
        CoreResult core = computeDeltaDebuggingCoreIfRequested(status=="NON_WELL_SEPARATED");
        auto coreEnd = std::chrono::steady_clock::now();
        coreSeconds = secondsBetween(coreStart,coreEnd);

        auto analysisEnd = std::chrono::steady_clock::now();
        const double elapsedSeconds = secondsBetween(analysisStart,analysisEnd);

        std::ostringstream json;
        json << "{\n";
        json << "  \"format_version\": " << jsonString("0.1") << ",\n";
        json << "  \"tool\": {\n";
        json << "    \"name\": " << jsonString("slugs") << ",\n";
        json << "    \"mode\": " << jsonString("checkWellSeparation") << ",\n";
        json << "    \"backend\": " << jsonString("slugs-native-bdd") << ",\n";
        json << "    \"algorithm\": " << jsonString("maoz-ringert-algorithm-1") << "\n";
        json << "  },\n";
        json << "  \"status\": " << jsonString(status) << ",\n";
        json << "  \"complete\": " << jsonBool(complete) << ",\n";
        json << std::fixed << std::setprecision(6);
        json << "  \"elapsed_time\": " << elapsedSeconds << ",\n";
        json << "  \"timing\": {\n";
        json << "    \"total\": " << elapsedSeconds << ",\n";
        json << "    \"reachability\": " << reachabilitySeconds << ",\n";
        json << "    \"safety_winning\": " << safetyWinningSeconds << ",\n";
        json << "    \"direct_safety_witness_region\": " << directSafetySeconds << ",\n";
        json << "    \"liveness_winning\": " << livenessWinningSeconds << ",\n";
        json << "    \"diagnostics_and_witness\": " << diagnosticsSeconds << ",\n";
        json << "    \"core_minimization\": " << coreSeconds << "\n";
        json << "  },\n";
        json << "  \"method\": " << jsonString("slugs-maoz-ringert-case-diagnosis") << ",\n";
        json << "  \"semantic_mode\": " << jsonString("slugs-mealy-transition-based") << ",\n";
        json << "  \"violated_assumption_type\": " << jsonString(violatedAssumptionType) << ",\n";
        json << "  \"cases\": [";
        for (unsigned int i=0;i<cases.size();i++) {
            if (i>0) json << ", ";
            json << jsonString(cases[i]);
        }
        json << "],\n";
        json << "  \"responsible_assumptions\": [";
        for (unsigned int i=0;i<responsibleAssumptions.size();i++) {
            if (i>0) json << ", ";
            json << "{";
            json << "\"kind\": " << jsonString(responsibleAssumptions[i].kind) << ", ";
            json << "\"index\": " << responsibleAssumptions[i].index << ", ";
            json << "\"source_index\": " << responsibleAssumptions[i].sourceIndex << ", ";
            json << "\"line\": " << responsibleAssumptions[i].line << ", ";
            json << "\"name\": " << jsonString(responsibleAssumptions[i].name);
            json << "}";
        }
        json << "],\n";
        json << "  \"responsible_assumption_mode\": " << jsonString("category_assumptions_not_minimal_core") << ",\n";
        writeWitnessJson(json,witness);
        writeCoreJson(json,core);
        json << "  \"checks\": {\n";
        json << "    \"env_init_contradiction\": " << jsonBool(envInitContradiction) << ",\n";
        json << "    \"env_safety_reachable_win\": " << jsonBool(safetyReachableWin) << ",\n";
        json << "    \"env_safety_all_initial_win\": " << jsonBool(safetyAllInitialWin) << ",\n";
        json << "    \"env_liveness_reachable_win\": " << jsonBool(livenessReachableWin) << ",\n";
        json << "    \"env_liveness_all_initial_win\": " << jsonBool(livenessAllInitialWin) << ",\n";
        json << "    \"env_liveness_checked\": " << jsonBool(livenessChecked) << ",\n";
        json << "    \"env_safety_formula_count\": " << safetyEnvFormulae.size() << ",\n";
        json << "    \"sys_safety_formula_count\": " << safetySysFormulae.size() << ",\n";
        json << "    \"env_liveness_formula_count\": " << livenessAssumptions.size() << ",\n";
        json << "    \"reachable_states_bdd_size\": " << reachableStates.getSize() << ",\n";
        json << "    \"safety_winning_states_bdd_size\": " << safetyWinningStates.getSize() << ",\n";
        json << "    \"direct_safety_violation_states_bdd_size\": " << directSafetyViolationStates.getSize() << ",\n";
        json << "    \"full_winning_states_bdd_size\": " << fullWinningStates.getSize() << "\n";
        json << "  },\n";
        json << "  \"warnings\": [";
        for (unsigned int i=0;i<warnings.size();i++) {
            if (i>0) json << ", ";
            json << jsonString(warnings[i]);
        }
        json << "]\n";
        json << "}\n";

        if (writeJsonToFile) {
            std::ofstream outFile(jsonOutputFilename.c_str());
            if (outFile.fail()) {
                SlugsException e(false);
                e << "Error: Cannot open well-separation JSON output file '" << jsonOutputFilename << "'.";
                throw e;
            }
            outFile << json.str();
        } else {
            std::cout << json.str();
        }
    }

    static GR1Context* makeInstance(std::list<std::string> &filenames) {
        return new XWellSeparation<T,writeJsonToFile,minimizeCore>(filenames);
    }
};

#endif
