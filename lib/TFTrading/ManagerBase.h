/************************************************************************
 * Copyright(c) 2011, One Unified. All rights reserved.                 *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

#pragma once

#include <map>

// class T:  target class

// Manager base class for 
//  AccountManager,
//  PortfolioManager
//  OrderManager, 
//  InstrumentManager, 
//  ProviderManager, 

#include <OUCommon/Singleton.h>

#include "Database.h"

namespace ou {
namespace tf { // TradeFrame

// T: CRTP base
template<class T> 
class ManagerBase: public ou::CSingleton<T> {
public:

  ManagerBase( void ) {};
  virtual ~ManagerBase( void ) {};

  void SetDbSession( ou::db::CSession::pSession_t& pDbSession ) {
    m_pDbSession = pDbSession;
  }

  virtual void RegisterTablesForCreation( void ) {};
  virtual void RegisterRowDefinitions( void ) {};
  virtual void PopulateTables( void ) {};

protected:
  // if session has been assigned, then persist records, if not, don't
  ou::db::CSession::pSession_t m_pDbSession;
private:
};

} // namespace tf
} // ou