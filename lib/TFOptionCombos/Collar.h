/************************************************************************
 * Copyright(c) 2020, One Unified. All rights reserved.                 *
 * email: info@oneunified.net                                           *
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
 * File:    Collar.h
 * Author:  raymond@burkholder.net
 * Project: TFOptionCombos
 * Created on July 19, 2020, 05:43 PM
 */

#ifndef COLLAR_H
#define COLLAR_H

#include <TFTrading/Position.h>
#include <TFTrading/Order_Combo.hpp>

#include "Combo.h"
#include "SpreadSpecs.h"

namespace ou { // One Unified
namespace tf { // TradeFrame
namespace option { // options

class Collar {
public:

  using mapChains_t = Combo::mapChains_t;
  using fLegSelected_t = Combo::fLegSelected_t;
  using pPosition_t = ou::tf::Position::pPosition_t;
  using pOrderCombo_t = ou::tf::OrderCombo::pOrderCombo_t;

  Collar();
  Collar( Collar&& ) = delete;
  Collar( const Collar& ) = delete;
  Collar& operator=( const Collar& rhs ) = delete;
  virtual ~Collar();

  static size_t LegCount();

  static void ChooseLegs( // throw Chain exceptions
    Combo::E20DayDirection
  , const mapChains_t& chains
  , boost::gregorian::date
  , const SpreadSpecs&
  , double priceUnderlying
  , const fLegSelected_t&
  );

  static void FillLegNote( size_t ix, Combo::E20DayDirection, LegNote::values_t& );

  static std::string Name(
    Combo::E20DayDirection
  , const mapChains_t& chains
  , boost::gregorian::date
  , const SpreadSpecs&
  , double price
  , const std::string& sUnderlying
  );

  static void AddLegOrder(
    const LegNote::Type
  , pOrderCombo_t
  , const ou::tf::OrderSide::EOrderSide
  , uint32_t nOrderQuantity
  , pPosition_t
  );

protected:
private:
};

} // namespace option
} // namespace tf
} // namespace ou

#endif /* COLLAR_H */
