/************************************************************************
 * Copyright(c) 2010, One Unified. All rights reserved.                 *
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

#include <string>
#include <vector>

#include "ChartEntryBase.h"

// level markers (horizontal lines at a price level)

namespace ou { // One Unified

class ChartEntryMark :
  public ChartEntryBase {
public:
  ChartEntryMark(void);
  virtual ~ChartEntryMark(void);
  void AddMark( double price, ou::Colour::enumColour colour, const std::string &name );
  virtual void AddEntryToChart( XYChart *pXY, structChartAttributes *pAttributes ) const;
protected:
  std::vector<ou::Colour::enumColour> m_vColour;
  std::vector<std::string> m_vName;
private:
};

} // namespace ou
