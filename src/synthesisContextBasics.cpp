#include "BF.h"
#include "gr1context.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <boost/algorithm/string.hpp>


/**
 * @brief Constructor that reads the problem instance from file and prepares the BFManager, the BFVarCubes, and the BFVarVectors
 * @param inFile the input filename
 */
GR1Context::GR1Context(std::list<std::string> &filenames) {
    (void)filenames;
}

/**
 * @brief Recurse internal function to parse a Boolean formula from a line in the input file
 * @param is the input stream from which the tokens in the line are read
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes, std::vector<BF> &memory) {
    std::string operation = "";
    is >> operation;
    if (operation=="") {
        SlugsException e(false);
        e << "Error reading line " << lineNumberCurrentlyRead << ". Premature end of line.";
        throw e;
    }
    if (operation=="|") return parseBooleanFormulaRecurse(is,allowedTypes,memory) | parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="^") return parseBooleanFormulaRecurse(is,allowedTypes,memory) ^ parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="&") return parseBooleanFormulaRecurse(is,allowedTypes,memory) & parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="!") return !parseBooleanFormulaRecurse(is,allowedTypes,memory);
    if (operation=="1") return mgr.constantTrue();
    if (operation=="0") return mgr.constantFalse();

    // Memory Functionality - Create Buffer
    if (operation=="$") {
        unsigned nofElements;
        is >> nofElements;
        if (is.fail()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number of memory elements.";
            throw e;
        }
        std::vector<BF> memoryNew(nofElements);
        for (unsigned int i=0;i<nofElements;i++) {
            memoryNew[i] = parseBooleanFormulaRecurse(is,allowedTypes,memoryNew);
        }
        return memoryNew[nofElements-1];
    }

    // Memory Functionality - Recall from Buffer
    if (operation=="?") {
        unsigned int element;
        is >> element;
        if (is.fail()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Expected number after memory recall operator '?'.";
            throw e;
        }
        if (element>=memory.size()) {
            SlugsException e(false);
            e << "Error reading line " << lineNumberCurrentlyRead << ". Trying to recall a memory element that has not been stored (yet).";
            throw e;
        }
        return memory[element];
    }

    // Has to be a variable!
    for (unsigned int i=0;i<variableNames.size();i++) {
        if (variableNames[i]==operation) {
            if (allowedTypes.count(variableTypes[i])==0) {
                SlugsException e(false);
                e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " is not allowed for this type of expression.";
                throw e;
            }
            return variables[i];
        }
    }
    SlugsException e(false);
    e << "Error reading line " << lineNumberCurrentlyRead << ". The variable " << operation << " has not been found.";
    throw e;
}

/**
 * @brief Internal function for parsing a Boolean formula from a line in the input file - calls the recursive function to do all the work.
 * @param currentLine the line to parse
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormula(std::string currentLine, std::set<VariableType> &allowedTypes) {

    std::istringstream is(currentLine);

    std::vector<BF> memory;
    BF result = parseBooleanFormulaRecurse(is,allowedTypes,memory);
    assert(memory.size()==0);
    std::string nextPart = "";
    is >> nextPart;
    if (nextPart=="") return result;

    SlugsException e(false);
    e << "Error reading line " << lineNumberCurrentlyRead << ". There are stray characters: '" << nextPart << "'";
    throw e;
}


void GR1Context::execute() {
    auto synthesisStart = std::chrono::steady_clock::now();
    checkRealizability();
    auto synthesisEnd = std::chrono::steady_clock::now();
    unsigned int apInputCount = 0;
    unsigned int apOutputCount = 0;
    for (unsigned int i = 0; i < variableTypes.size(); i++) {
        if (variableTypes[i] == PreInput) apInputCount++;
        if (variableTypes[i] == PreOutput) apOutputCount++;
    }
    if (realizable) {
        std::cerr << "RESULT: Specification is realizable.\n";
    } else {
        std::cerr << "RESULT: Specification is unrealizable.\n";
    }

    const double synthesisSeconds = std::chrono::duration<double>(synthesisEnd - synthesisStart).count();
    std::cerr << "Timing:\n";
    std::cerr << std::fixed << std::setprecision(6);
    std::cerr << "  - Synthesis time (checkRealizability only): " << synthesisSeconds << " s\n";
    std::cerr << "Experiment summary:\n";
    std::cerr << "  - AP_I (PreInput vars): " << apInputCount << "\n";
    std::cerr << "  - AP_O (PreOutput vars): " << apOutputCount << "\n";
    std::cerr << "  - |ENV_TRANS|: " << safetyEnvFormulae.size() << "\n";
    std::cerr << "  - |SYS_TRANS|: " << safetySysFormulae.size() << "\n";
    std::cerr << "  - |ENV_LIVENESS|: " << livenessAssumptions.size() << "\n";
    std::cerr << "  - |SYS_LIVENESS|: " << livenessGuarantees.size() << "\n";
    std::cerr << "  - CUDD live node count: " << Cudd_ReadNodeCount(mgr.getMgr()) << "\n";
    std::cerr << "  - CUDD peak node count: " << Cudd_ReadPeakNodeCount(mgr.getMgr()) << "\n";
    std::cerr << "  - CUDD manager var count: " << Cudd_ReadSize(mgr.getMgr()) << "\n";
    std::cerr << "  - CUDD reorderings: " << Cudd_ReadReorderings(mgr.getMgr()) << "\n";
    std::cerr << "  - CUDD reordering time: " << Cudd_ReadReorderingTime(mgr.getMgr()) << " ms\n";
    std::cerr << "  - CUDD garbage collections: " << Cudd_ReadGarbageCollections(mgr.getMgr()) << "\n";
    std::cerr << "  - CUDD GC time: " << Cudd_ReadGarbageCollectionTime(mgr.getMgr()) << " ms\n";
    std::cerr << "  - CUDD memory in use: " << Cudd_ReadMemoryInUse(mgr.getMgr()) << " bytes\n";

    // Compact per-run metrics for cross-spec comparison.
    auto metricsStart = std::chrono::steady_clock::now();
    std::cerr << "Complexity metrics (this run):\n";
    std::cerr << "  1) Manager live-node count: " << Cudd_ReadNodeCount(mgr.getMgr()) << "\n";
    std::cerr << "     Meaning: current total nodes stored in the CUDD manager unique table.\n";
    std::cerr << "  2) Winning-region DAG size: " << winningPositions.getSize() << "\n";
    std::cerr << "     Meaning: size of the BDD representing the computed winning set.\n";
    double strategyMetricSeconds = 0.0;
    if (realizable) {
        auto strategyMetricStart = std::chrono::steady_clock::now();
        BF strategyRelation = mgr.constantFalse();
        for (unsigned int i = 0; i < strategyDumpingData.size(); i++) {
            strategyRelation |= strategyDumpingData[i].second;
        }
        auto strategyMetricEnd = std::chrono::steady_clock::now();
        strategyMetricSeconds = std::chrono::duration<double>(strategyMetricEnd - strategyMetricStart).count();
        std::cerr << "  3) Strategy BDD DAG size: " << strategyRelation.getSize() << "\n";
        std::cerr << "     Meaning: merged size of dumped strategy transition BDDs.\n";
    } else {
        std::cerr << "  3) Strategy BDD DAG size: N/A (specification unrealizable)\n";
    }

    std::cerr << "Detailed CUDD manager stats follow (global across all BDDs in this run):\n";
    std::cerr << "  - unique table/cache sizes and usage reflect total manager load\n";
    std::cerr << "  - node/memory peaks show worst-case pressure during synthesis\n";
    std::cerr << "  - these are manager-wide, not a single-formula size metric\n";
    mgr.printStats(true);
    auto metricsEnd = std::chrono::steady_clock::now();
    const double metricsSeconds = std::chrono::duration<double>(metricsEnd - metricsStart).count();
    std::cerr << "Timing:\n";
    std::cerr << "  - Metrics reporting time (all of the above, including Cudd_PrintInfo): " << metricsSeconds << " s\n";
    if (realizable) {
        std::cerr << "  - Strategy metric construction sub-time (#3 only): " << strategyMetricSeconds << " s\n";
    }
}

void GR1Context::init(std::list<std::string> &filenames) {
    if (filenames.size()==0) {
        throw "Error: Cannot load SLUGS input file - there has been no input file name given!";
    }

    std::string inFileName = filenames.front();
    filenames.pop_front();

    // Open input file or produce error message if that does not work
    std::ifstream inFile(inFileName.c_str());
    if (inFile.fail()) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot open input file '" << inFileName << "'";
        throw errorMessage.str();
    }

    // Prepare safety and initialization constraints
    initEnv = mgr.constantTrue();
    initSys = mgr.constantTrue();
    safetyEnv = mgr.constantTrue();
    safetySys = mgr.constantTrue();

    // The readmode variable stores in which chapter of the input file we are
    int readMode = -1;
    std::string currentLine;
    lineNumberCurrentlyRead = 0;
    while (std::getline(inFile,currentLine)) {
        lineNumberCurrentlyRead++;
        boost::trim(currentLine);
        if ((currentLine.length()>0) && (currentLine[0]!='#')) {
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
                    initEnv &= parseBooleanFormula(currentLine,allowedTypes);
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
            }
        }
    }

    // Check if variable names have been used twice
    std::set<std::string> variableNameSet(variableNames.begin(),variableNames.end());
    if (variableNameSet.size()!=variableNames.size()) throw SlugsException(false,"Error in input file: some variable name has been used twice!\nPlease keep in mind that for every variable used, a second one with the same name but with a \"'\" appended to it is automacically created.");

    // Make sure that there is at least one liveness assumption and one liveness guarantee
    // The synthesis algorithm might be unsound otherwise
    if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
    if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
}
