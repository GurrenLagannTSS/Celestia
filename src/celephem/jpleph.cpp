// jpleph.cpp
//
// Copyright (C) 2004, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// Load JPL's DE200, DE405, and DE406 ephemerides and compute planet
// positions.

#include <fstream>
#include <iomanip>
#include <cassert>
#include <celutil/bytes.h>
#include "jpleph.h"

using namespace Eigen;
using namespace std;

constexpr const unsigned int NConstants         =  400;
constexpr const unsigned int ConstantNameLength =  6;

constexpr const unsigned int MaxChebyshevCoeffs = 32;

constexpr const unsigned int LabelSize = 84;

constexpr const unsigned int INPOP_DE_COMPATIBLE = 100;
constexpr const unsigned int DE200 = 200;

// Read a big-endian or little endian 32-bit unsigned integer
static uint32_t readUint(istream& in, bool swap)
{
    uint32_t ret;
    in.read((char*) &ret, sizeof(uint32_t));
    return swap ? bswap_32(ret) : ret;
}

// Read a big-endian or little endian 64-bit IEEE double.
// If the native double format isn't IEEE 754, there will be troubles.
static double readDouble(istream& in, bool swap)
{
    double d;
    in.read((char*) &d, sizeof(double));
    return swap ? bswap_double(d) : d;
}


JPLEphRecord::~JPLEphRecord()
{
    delete coeffs;
}


unsigned int JPLEphemeris::getDENumber() const
{
    return DENum;
}

double JPLEphemeris::getStartDate() const
{
    return startDate;
}

double JPLEphemeris::getEndDate() const
{
    return endDate;
}

unsigned int JPLEphemeris::getRecordSize() const
{
    return recordSize;
}

bool JPLEphemeris::getByteSwap() const
{
    return swapBytes;
}

// Return the position of an object relative to the solar system barycenter
// or the Earth (in the case of the Moon) at a specified TDB Julian date tjd.
// If tjd is outside the span covered by the ephemeris it is clamped to a
// valid time.
Vector3d JPLEphemeris::getPlanetPosition(JPLEphemItem planet, double tjd) const
{
    // Solar system barycenter is the origin
    if (planet == JPLEph_SSB)
    {
        return Vector3d::Zero();
    }

    // The position of the Earth must be computed from the positions of the
    // Earth-Moon barycenter and Moon
    if (planet == JPLEph_Earth)
    {
        Vector3d embPos = getPlanetPosition(JPLEph_EarthMoonBary, tjd);

        // Get the geocentric position of the Moon
        Vector3d moonPos = getPlanetPosition(JPLEph_Moon, tjd);

        return embPos - moonPos * (1.0 / (earthMoonMassRatio + 1.0));
    }

    // Clamp time to [ startDate, endDate ]
    if (tjd < startDate)
        tjd = startDate;
    else if (tjd > endDate)
    tjd = endDate;

    // recNo is always >= 0:
    auto recNo = (unsigned int) ((tjd - startDate) / daysPerInterval);
    // Make sure we don't go past the end of the array if t == endDate
    if (recNo >= records.size())
        recNo = records.size() - 1;
    const JPLEphRecord* rec = &records[recNo];

    assert(coeffInfo[planet].nGranules >= 1);
    assert(coeffInfo[planet].nGranules <= 32);
    assert(coeffInfo[planet].nCoeffs <= MaxChebyshevCoeffs);

    // u is the normalized time (in [-1, 1]) for interpolating
    // coeffs is a pointer to the Chebyshev coefficients
    double u = 0.0;
    double* coeffs = nullptr;

    // nGranules is unsigned int so it will be compared against FFFFFFFF:
    if (coeffInfo[planet].nGranules == (unsigned int) -1)
    {
        coeffs = rec->coeffs + coeffInfo[planet].offset;
        u = 2.0 * (tjd - rec->t0) / daysPerInterval - 1.0;
    }
    else
    {
        double daysPerGranule = daysPerInterval / coeffInfo[planet].nGranules;
        auto granule = (int) ((tjd - rec->t0) / daysPerGranule);
        double granuleStartDate = rec->t0 + daysPerGranule * (double) granule;
        coeffs = rec->coeffs + coeffInfo[planet].offset +
                 granule * coeffInfo[planet].nCoeffs * 3;
        u = 2.0 * (tjd - granuleStartDate) / daysPerGranule - 1.0;
    }

    // Evaluate the Chebyshev polynomials
    double sum[3];
    double cc[MaxChebyshevCoeffs];
    unsigned int nCoeffs = coeffInfo[planet].nCoeffs;
    for (int i = 0; i < 3; i++)
    {
        cc[0] = 1.0;
        cc[1] = u;
        sum[i] = coeffs[i * nCoeffs] + coeffs[i * nCoeffs + 1] * u;
        for (unsigned int j = 2; j < nCoeffs; j++)
        {
            cc[j] = 2.0 * u * cc[j - 1] - cc[j - 2];
            sum[i] += coeffs[i * nCoeffs + j] * cc[j];
        }
    }

    return Vector3d(sum[0], sum[1], sum[2]);
}

#pragma pack(push, 1)
struct JPLECoeff
{
    uint32_t offset;
    uint32_t nCoeffs;
    uint32_t nGranules;
};
struct JPLEFileHeader
{
    //  Three header labels
    char headerLabels[3][LabelSize];

    // Constant names
    char constantNames[NConstants][ConstantNameLength];

    // Start time, end time, and time interval
    double startDate, endDate, daysPerInterval;

    // Number of constants with valid values
    uint32_t nConstants;

    // km per AU
    double au;
    // Earth-Moon mass ratio
    double earthMoonMassRatio;

    // Coefficient information for each item in the ephemeris
    JPLECoeff coeffInfo[JPLEph_NItems];

    // DE number
    uint32_t deNum;

    // Libration coefficient information
    JPLECoeff librationCoeffInfo;
};
#pragma pack(pop)

#define MAYBE_SWAP_DOUBLE(d) (swapBytes ? bswap_double(d) : (d))
#define MAYBE_SWAP_UINT32(u) (swapBytes ? bswap_32(u) : (u))

JPLEphemeris* JPLEphemeris::load(istream& in)
{
    JPLEFileHeader fh;
    in.read((char*) &fh, sizeof(fh));
    if (!in.good())
        return nullptr;

    uint32_t deNum = fh.deNum;
    uint32_t deNum2 = bswap_32(deNum);

    bool swapBytes;
    if (deNum == INPOP_DE_COMPATIBLE)
    {
        // INPOP ephemeris with same endianess as CPU
        swapBytes = false;
    }
    else if (deNum2 == INPOP_DE_COMPATIBLE)
    {
        // INPOP ephemeris with different endianess
        swapBytes = true;
        deNum = deNum2;
    }
    else if ((deNum > (1u << 15)) && (deNum2 >= DE200))
    {
        // DE ephemeris with different endianess
        swapBytes = true;
        deNum = deNum2;
    }
    else if ((deNum <= (1u << 15)) && (deNum >= DE200))
    {
        // DE ephemeris with same endianess as CPU
        swapBytes = false;
    }
    else
    {
        // something unknown or broken
        return nullptr;
    }

    auto *eph = new JPLEphemeris();
    eph->swapBytes = swapBytes;
    eph->DENum = deNum;

    // Read the start time, end time, and time interval
    eph->startDate          = MAYBE_SWAP_DOUBLE(fh.startDate);
    eph->endDate            = MAYBE_SWAP_DOUBLE(fh.endDate);
    eph->daysPerInterval    = MAYBE_SWAP_DOUBLE(fh.daysPerInterval);
    // kilometers per astronomical unit
    eph->au                 = MAYBE_SWAP_DOUBLE(fh.au);
    eph->earthMoonMassRatio = MAYBE_SWAP_DOUBLE(fh.earthMoonMassRatio);

    // Read the coefficient information for each item in the ephemeris
    eph->recordSize = 0;
    for (unsigned int i = 0; i < JPLEph_NItems; i++)
    {
        eph->coeffInfo[i].offset        = MAYBE_SWAP_UINT32(fh.coeffInfo[i].offset) - 3;
        eph->coeffInfo[i].nCoeffs       = MAYBE_SWAP_UINT32(fh.coeffInfo[i].nCoeffs);
        eph->coeffInfo[i].nGranules     = MAYBE_SWAP_UINT32(fh.coeffInfo[i].nGranules);
        // last item is the nutation ephemeris (only 2 components)
        unsigned nRecords = i == JPLEph_NItems - 1 ? 2 : 3;
        eph->recordSize += eph->coeffInfo[i].nCoeffs * eph->coeffInfo[i].nGranules * nRecords;
    }

    eph->librationCoeffInfo.offset      = MAYBE_SWAP_UINT32(fh.librationCoeffInfo.offset);
    eph->librationCoeffInfo.nCoeffs     = MAYBE_SWAP_UINT32(fh.librationCoeffInfo.nCoeffs);
    eph->librationCoeffInfo.nGranules   = MAYBE_SWAP_UINT32(fh.librationCoeffInfo.nGranules);
    eph->recordSize += eph->librationCoeffInfo.nCoeffs * eph->librationCoeffInfo.nGranules * 3;
    eph->recordSize += 2;   // record start and end time

    // if INPOP ephemeris, read record size
    if (deNum == INPOP_DE_COMPATIBLE)
    {
       eph->recordSize = readUint(in, eph->swapBytes);
       // Skip past the rest of the record
       in.ignore(eph->recordSize * 8 - sizeof(JPLEFileHeader) - sizeof(uint32_t));
    }
    else
    {
        // Skip past the rest of the record
        in.ignore(eph->recordSize * 8 - sizeof(JPLEFileHeader));
    }

    // The next record contains constant values (which we don't need)
    in.ignore(eph->recordSize * 8);
    if (!in.good())
    {
        delete eph;
        return nullptr;
    }

    auto nRecords = (unsigned int) ((eph->endDate - eph->startDate) /
                        eph->daysPerInterval);
    eph->records.resize(nRecords);
    for (unsigned i = 0; i < nRecords; i++)
    {
        eph->records[i].t0 = readDouble(in, eph->swapBytes);
        eph->records[i].t1 = readDouble(in, eph->swapBytes);

        // Allocate coefficient array for this record; the first two
        // 'coefficients' are actually the start and end time (t0 and t1)
        eph->records[i].coeffs = new double[eph->recordSize - 2];
        for (unsigned int j = 0; j < eph->recordSize - 2; j++)
            eph->records[i].coeffs[j] = readDouble(in, eph->swapBytes);

        // Make sure that we read this record successfully
        if (!in.good())
        {
            delete eph;
            return nullptr;
        }
    }

    return eph;
}
