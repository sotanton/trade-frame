/************************************************************************
 * Copyright(c) 2021, One Unified. All rights reserved.                 *
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

/*
 * File:    SymbolLookup.h
 * Author:  raymond@burkholder.net
 * Project: TFIQFeed
 * Created: Novemeber 2, 2021, 11:09
 */

#pragma once

#include <map>
#include <string>
#include <functional>

#include <OUCommon/Network.h>
#include <OUCommon/KeyWordMatch.h>

#include "SecurityType.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace iqfeed { // IQFeed

class SymbolLookup
  : public ou::Network<SymbolLookup> {
    friend ou::Network<SymbolLookup>;
public:

  using inherited_t = ou::Network<SymbolLookup>;
  using linebuffer_t = inherited_t::linebuffer_t;

  struct ListedMarket {
    //uint16_t idListedMarket;
    std::string sShortName;
    std::string sLongName;
    std::string idGroup;
    std::string sGroupName;
  };

  using mapListedMarket_t = std::map<uint16_t,ListedMarket>; // key = idListedMarket

  struct SecurityType {
    //uint16_t idSecurityType;
    ESecurityType eSecurityType;
    std::string sShortName;
    std::string sLongName;
  };

  using mapSecurityType_t = std::map<uint16_t,SecurityType>; // key = idSecurityType

  struct TradeCondition {
    // uint16_t idTradeCondition
    std::string sShortName;
    std::string sLongName;
  };

  using mapTradeCondition_t = std::map<uint16_t,TradeCondition>; // key = idTradeCondition

  using fDone_t = std::function<void()>;

  SymbolLookup(
    mapListedMarket_t&,
    mapSecurityType_t&,
    mapTradeCondition_t&,
    fDone_t&&
    );
  virtual ~SymbolLookup();

  void Connect();
  void Disconnect();

  using setNames_t = std::set<std::string>;
  using fSymbol_t = std::function<void(const std::string&)>;
  void SymbolList( 
    const setNames_t& setExchangeFilter, const setNames_t& setSecurityTypeFilter,
    fSymbol_t&&, fDone_t&&
    );

protected:

  // called by Network via CRTP
  void OnNetworkConnected();
  void OnNetworkDisconnected();
  void OnNetworkError( size_t e );
  void OnNetworkSendDone();
  void OnNetworkLineBuffer( linebuffer_t* );  // new line available for processing

private:

  ou::KeyWordMatch<uint16_t> m_kwmListedMarket;
  ou::KeyWordMatch<uint16_t> m_kwmSecurityType;
  ou::KeyWordMatch<uint16_t> m_kwmTradeCondition;

  mapListedMarket_t& m_mapListedMarket;
  mapSecurityType_t& m_mapSecurityType;
  mapTradeCondition_t& m_mapTradeCondition;

  using setIdSecurityType_t = std::set<uint16_t>;
  setIdSecurityType_t m_setIdSecurityType; // computed for each SBF query

  fSymbol_t m_fSymbol;
  fDone_t m_fDoneConnection;
  fDone_t m_fDoneSymbolList;

  void MapSecurityTypes();

  void BuildKeyWords();

};

} // namespace iqfeed
} // namespace tf
} // namespace ou
