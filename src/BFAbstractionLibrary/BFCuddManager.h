/*
 * BFCuddManager.h
 *
 *  Created on: 06.08.2010
 *      Author: ehlers
 */

#ifndef BFCUDDMANAGER_H_
#define BFCUDDMANAGER_H_

#include <cassert>
#include <vector>
#include <boost/utility.hpp>
#include <iostream>
#include <list>
#include <cstdio>
#include <cstdlib>

#include <cudd.h>

class BFBdd;
class BFBddVarCube;
class BFBddVarVector;

class BFBddManager: boost::noncopyable {
private:
	DdManager *mgr;
	static bool reorderingEnabledForNewManagers;
	static unsigned int reorderingThresholdForNewManagers;

public:
	BFBddManager(unsigned int maxMemoryInMB = 3096, float reorderingMaxBlowup = 1.2f, bool enableReordering = BFBddManager::reorderingEnabledForNewManagers, unsigned int reorderingThreshold = BFBddManager::reorderingThresholdForNewManagers);
	~BFBddManager();

	// Controls whether CUDD dynamic reordering (sifting) is turned on for BFBddManager
	// instances constructed from this point onwards via the default 'enableReordering'
	// argument above. Intended to be called once, before any BFBddManager is constructed
	// (e.g. right after command-line parsing), since it has no effect on managers that
	// already exist.
	static void setReorderingEnabledForNewManagers(bool enable) {
		reorderingEnabledForNewManagers = enable;
	}

	// Controls the CUDD live-node threshold (Cudd_SetNextReordering) at which the
	// *first* automatic reordering fires for BFBddManager instances constructed from
	// this point onwards, via the default 'reorderingThreshold' argument above. A
	// value of 0 means "leave CUDD's own default (DD_FIRST_REORDER, 4004 nodes)".
	// Intended to be called once, before any BFBddManager is constructed, same as
	// setReorderingEnabledForNewManagers().
	static void setReorderingThresholdForNewManagers(unsigned int threshold) {
		reorderingThresholdForNewManagers = threshold;
	}

	void setAutomaticOptimisation(bool enable);
    void setReorderingMaxBlowup(float reorderingMaxBlowup);
	BFBddVarCube computeCube(const BFBdd * vars, const int * phase, int n) const;
	BFBddVarCube computeCube(const std::vector<BFBdd> &vars) const;
	BFBddVarVector computeVarVector(const std::vector<BFBdd> &vars) const;
    BFBdd readBDDFromFile(const char *filename, std::vector<BFBdd> &vars) const;
    void writeBDDToFile(const char *filename, std::string fileprefix, BFBdd bdd, std::vector<BFBdd> &vars, std::vector<std::string> variableNames) const;
	//void groupVariables(const std::vector<BFBdd> &which);
	void printStats(bool toStdErr = false);

	inline BFBdd constantTrue() const;
	inline BFBdd constantFalse() const;
	inline BFBdd newVariable();
	inline BFBdd multiAnd(const std::vector<BFBdd> &parts) const;
	inline BFBdd multiOr(const std::vector<BFBdd> &parts) const;

	inline DdManager *getMgr() const {
		return mgr;
	}

	friend BFBddVarVector __make_bdd_var_vect(const std::vector<BFBdd>& from, BFBddManager* mgr);
	friend class BFBdd;
	friend class BFBddVarCube;
	friend class BFBddVarVector;
};

#endif
