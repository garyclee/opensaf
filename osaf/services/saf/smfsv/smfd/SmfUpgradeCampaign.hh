/*
 *
 * (C) Copyright 2009 The OpenSAF Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. This file and program are licensed
 * under the GNU Lesser General Public License Version 2.1, February 1999.
 * The complete license can be accessed from the following location:
 * http://opensource.org/licenses/lgpl-license.php
 * See the Copying file included with the OpenSAF distribution for full
 * licensing terms.
 *
 * Author(s): Ericsson AB
 *
 */

#ifndef __SMFUPGRADECAMPAIGN_H
#define __SMFUPGRADECAMPAIGN_H

/* ========================================================================
 *   INCLUDE FILES
 * ========================================================================
 */
#include <string>
#include <vector>
#include <saSmf.h>
#include "SmfCampaignInit.hh"
#include "SmfCampaignWrapup.hh"

class SmfUpgradeProcedure;
class SmfCampState;
class SmfUpgradeAction;

/* ========================================================================
 *   DEFINITIONS
 * ========================================================================
 */

/* ========================================================================
 *   TYPE DEFINITIONS
 * ========================================================================
 */

/* ========================================================================
 *   DATA DECLARATIONS
 * ========================================================================
 */

///
/// Purpose: Contains all campaign data extracted from the campaign xml file.
///
class SmfUpgradeCampaign {
 public:

///
/// Purpose: Constructor.
/// @param   None
/// @return  None
///
	SmfUpgradeCampaign();

///
/// Purpose: Destructor.
/// @param   None
/// @return  None
///
	~SmfUpgradeCampaign();

///
/// Purpose: Get the class name.
/// @param   io_str A reference to a std::string for the method to fill in.
/// @return  None.
///
	void getClassName(std::string & io_str) const;

///
/// Purpose: Get a string representation of the upgrade campaign.
/// @param   io_str A reference to a std::string for the method to fill in.
/// @return  None.
///
	void toString(std::string & io_str) const;

///
/// Purpose: Set the name of the upgrade campaign package.
/// @param   A std::string containing the package name.
/// @return  None.
///
	void setCampPackageName(const std::string & i_name);

///
/// Purpose: Get the name of the upgrade campaign package.
/// @param   None.
/// @return  A std::string containing the package name.
///
	const std::string & getCampPackageName(void);

///
/// Purpose: Set the campaign name.
/// @param   i_name A reference to a std::string containing the package name.
/// @return  None.
///
	void setCampaignName(const std::string & i_name);

///
/// Purpose: Get the campaign name.
/// @param   None.
/// @return  A std::string containing the campaign name.
///
	const std::string & getCampaignName(void);

///
/// Purpose: Set the xsi.
/// @param   i_name A reference to a std::string containing the xsi name.
/// @return  None.
///
	void setXsi(const std::string & i_name);

///
/// Purpose: Set the name space schema location.
/// @param   i_name A reference to a std::string containing the name space.
/// @return  None.
///
	void setNameSpaceSchemaLocation(const std::string & i_name);

///
/// Purpose: Set the campaign period.
/// @param   i_period A reference to a std::string containing the campaign period (in nano seconds).
/// @return  None.
///
	void setCampaignPeriod(const std::string & i_period);

///
/// Purpose: Set the configuration base.
/// @param   i_confbase A reference to a std::string containing the configuration base.
/// @return  None.
///
	void setConfigurationBase(const std::string & i_confbase);

///
/// Purpose: Set current campaign state.
/// @param   i_state A SaSmfCmpgStateT object.
/// @return  None.
///
	void setCampState(SaSmfCmpgStateT i_state);

///
/// Purpose: Add one SmfUpgradeProcedure to the campaign.
/// @param   i_procedure A pointer to a (filled in) SmfUpgradeProcedure object that should be added.
/// @return  None.
///
	void addUpgradeProcedure(SmfUpgradeProcedure * i_procedure);

///
/// Purpose: Get pointers to the upgrade procedures.
/// @param   None.
/// @return  A vector containing pointers to SmfUpgradeProcedure objecs.
///
	const std::vector < SmfUpgradeProcedure * >& getUpgradeProcedures();

///
/// Purpose: Sort the added SmfUpgradeProcedure in execution level order.
/// @param   None.
/// @return  None.
///
	void sortProceduresInExecLevelOrder();

///
/// Purpose: Add one "add to IMM" campaign init operation to the campaign.
/// @param   i_operation A pointer to a (filled in) SmfImmOperation object that should be added.
/// @return  None.
///
	void addCampInitAddToImm(SmfImmOperation * i_operation);

///
/// Purpose: Add one campaign init action to the campaign.
/// @param   i_action A pointer to a (filled in) SmfUpgradeAction object that should be added.
/// @return  None.
///
	void addCampInitAction(SmfUpgradeAction * i_action);

///
/// Purpose: Add one campaign wrapup operation to the campaign.
/// @param   i_operation A pointer to a (filled in) SmfImmOperation object that should be added.
/// @return  None.
///
	void addCampWrapupRemoveFromImm(SmfImmOperation * i_operation);

///
/// Purpose: Add one campaign complete action to the campaign.
/// @param   i_action A pointer to a (filled in) SmfUpgradeAction object that should be added.
/// @return  None.
///
	void addCampCompleteAction(SmfUpgradeAction * i_action);

///
/// Purpose: Add one campaign wrapup action to the campaign.
/// @param   i_action A pointer to a (filled in) SmfUpgradeAction object that should be added.
/// @return  None.
///
	void addCampWrapupAction(SmfUpgradeAction * i_action);

///
/// Purpose: Set time for waitToCommit.
/// @param   i_time The time before commit is permitted.
/// @return  None.
///
	void setWaitToCommit(SaTimeT i_time);

///
/// Purpose: Get time for waitToCommit.
/// @param   i_time The time before commit is permitted. In nano sec.
/// @return  SaTimeT
///
	SaTimeT getWaitToCommit();

///
/// Purpose: Set time for waitToAllowNewCampaign.
/// @param   i_time The time before commit is permitted.
/// @return  None.
///
	void setWaitToAllowNewCampaign(SaTimeT i_time);

///
/// Purpose: Get time for waitToAllowNewCampaign.
/// @param   i_time The time before commit is permitted. In nano sec.
/// @return  SaTimeT
///
	SaTimeT getWaitToAllowNewCampaign();

///
/// Purpose: Start or continue the execution of the campaign.
/// @param   None.
/// @return  None.
///
	void execute();

///
/// Purpose: Execute the campaign initialization part.
/// @param   None.
/// @return  None.
///
	void executeInit();

///
/// Purpose: Execute the campaign upgrade procedures.
/// @param   None.
/// @return  None.
///
	void executeProc();

///
/// Purpose: Execute campaign wrapup part.
/// @param   None.
/// @return  None.
///
	void executeWrapup();

///
/// Purpose: Rollback the campaign.
/// @param   None.
/// @return  None.
///
	void rollback();

///
/// Purpose: Suspend the campaign.
/// @param   None.
/// @return  None.
///
	void suspend();

///
/// Purpose: Commit the campaign.
/// @param   None.
/// @return  None.
///
	void commit();

	friend class SmfCampState;
	friend class SmfCampStateInitial;
	friend class SmfCampStateExecuting;
	friend class SmfCampStateExecCompleted;
	friend class SmfCampStateSuspendingExec;
	friend class SmfCampStateExecSuspended;
	friend class SmfCampStateCommitted;
	friend class SmfCampStateExecFailed;
	friend class SmfCampStateErrorDetected;

 private:

	void changeState(const SmfCampState * i_state);

//--------------------------------------------------------------------------
// Purpose:  Disables copy constructor
//--------------------------------------------------------------------------
	 SmfUpgradeCampaign(const SmfUpgradeCampaign &);

//--------------------------------------------------------------------------
// Purpose:  Disables assignment operator
//--------------------------------------------------------------------------
	 SmfUpgradeCampaign & operator=(const SmfUpgradeCampaign &);

//------------------------------------------------------------------------------
// Purpose:  Execute the procedures within the campaign
// Comments: -
// @param    None
// @return   None
//------------------------------------------------------------------------------
	void procedureExecute();

	SmfCampState *m_state;
        std::string m_campPackageName;
        std::string m_campaignName;
        std::string m_noNameSpaceSchemaLocation;
        std::string m_xsi;
        std::string m_campaignPeriod;
        std::string m_configurationBase;
	SmfCampaignInit m_campInit;
	SmfCampaignWrapup m_campWrapup;
        std::vector < SmfUpgradeProcedure * >m_procedure;
        SaTimeT m_waitToCommit;
        SaTimeT m_waitToAllowNewCampaign;
	int m_noOfExecutingProc;
};
#endif				// __SMFUPGRADECAMPAIGN_H
