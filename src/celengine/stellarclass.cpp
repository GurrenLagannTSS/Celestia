// stellarclass.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <cstring>
#include <celutil/debug.h>
#include <cassert>
#include <config.h>
#include "stellarclass.h"

using namespace std;


Color StellarClass::getApparentColor() const
{
    return getApparentColor(getSpectralClass());
}


Color StellarClass::getApparentColor(StellarClass::SpectralClass sc) const
{
    switch (sc)
    {
    case Spectral_O:
        return Color(0.7f, 0.8f, 1.0f);
    case Spectral_B:
        return Color(0.8f, 0.9f, 1.0f);
    case Spectral_A:
        return Color(1.0f, 1.0f, 1.0f);
    case Spectral_F:
        return Color(1.0f, 1.0f, 0.88f);
    case Spectral_G:
        return Color(1.0f, 1.0f, 0.75f);
    case StellarClass::Spectral_K:
        return Color(1.0f, 0.9f, 0.7f);
    case StellarClass::Spectral_M:
        return Color(1.0f, 0.7f, 0.7f);
    case StellarClass::Spectral_R:
    case StellarClass::Spectral_S:
    case StellarClass::Spectral_N:
    case StellarClass::Spectral_C:
        return Color(1.0f, 0.4f, 0.4f);
    case StellarClass::Spectral_L:
    case StellarClass::Spectral_T:
        return Color(0.75f, 0.2f, 0.2f);
    case StellarClass::Spectral_Y:
        return Color(0.5f, 0.175f, 0.125f);
    default:
        // TODO: Figure out reasonable colors for Wolf-Rayet stars,
        // white dwarfs, and other oddities
        return Color(1.0f, 1.0f, 1.0f);
    }
}


// The << method of converting the stellar class to a string is
// preferred, but it's not always practical, especially when you've
// got a completely broken implementation of stringstreams to
// deal with (*cough* gcc *cough*).
string StellarClass::str() const
{
    char s0, s1;
    const char* s2 = "";

    switch (getStarType())
    {
    case StellarClass::WhiteDwarf:
        s0 = "WD"[(unsigned int) getSpectralClass()];
        s1 = "0123456789"[getSubclass()];
    case StellarClass::NeutronStar:
        s0 = "Q"[(unsigned int) getSpectralClass()];
        s1 = "0123456789"[getSubclass()];
    case StellarClass::BlackHole:
        return "X";
    case StellarClass::NormalStar:
        s0 = "OBAFGKMRSNWW?LTYC"[(unsigned int) getSpectralClass()];
        s1 = "0123456789"[getSubclass()];
        switch (getLuminosityClass())
        {
        case StellarClass::Lum_Ia0:
            s2 = " I-a0";
            break;
        case StellarClass::Lum_Ia:
            s2 = " I-a";
            break;
        case StellarClass::Lum_Ib:
            s2 = " I-b";
            break;
        case StellarClass::Lum_II:
            s2 = " II";
            break;
        case StellarClass::Lum_III:
            s2 = " III";
            break;
        case StellarClass::Lum_IV:
            s2 = " IV";
            break;
        case StellarClass::Lum_V:
            s2 = " V";
            break;
        case StellarClass::Lum_VI:
            s2 = " VI";
            break;
        default: break;  // Do nothing, but prevent GCC4 warnings (Beware: potentially dangerous)
        }
        return fmt::sprintf("%c%c%s", s0, s1, s2);
    }

    return "?";
}


uint16_t
StellarClass::packV1() const
{
    // StarDB Ver. 0x0100 doesn't support Spectral_Y.
    // Classes following Spectral_Y are shifted by 1.
    uint16_t sc;
    if (specClass == SpectralClass::Spectral_Y)
        sc = (uint16_t) SpectralClass::Spectral_Unknown;
    else
        sc = (uint16_t) specClass > SpectralClass::Spectral_Y ? specClass - 1 : specClass;

    return (((uint16_t) starType << 12) |
           (((uint16_t) sc & 0x0f) << 8) |
           ((uint16_t) subclass << 4) |
           ((uint16_t) lumClass));
}


uint16_t
StellarClass::packV2() const
{
    uint16_t sc = (starType == StellarClass::WhiteDwarf ? specClass - 1 : specClass);

    return (((uint16_t) starType         << 18) |
           (((uint16_t) sc       & 0x1f) << 8)  |
           (((uint16_t) subclass & 0x0f) << 4)  |
           ((uint16_t)  lumClass & 0x0f));
}


bool
StellarClass::unpackV1(uint16_t st)
{
    starType = static_cast<StellarClass::StarType>(st >> 13);

    switch (starType)
    {
    case NormalStar:
        specClass = static_cast<SpectralClass>(st >> 8 & 0xf);
        // StarDB Ver. 0x0100 doesn't support Spectral_Y
        // Spectral_Y has the value Spectral_C had earlier.
        if (specClass == SpectralClass::Spectral_Y)
            specClass = SpectralClass::Spectral_C;
        subclass = st >> 4 & 0xf;
        lumClass = static_cast<LuminosityClass>(st & 0xf);
        break;
    case WhiteDwarf:
        if ((st >> 8 & 0xf) >= WDClassCount)
            return false;
        specClass = static_cast<SpectralClass>((st >> 8 & 0xf) + SpectralClass::Spectral_DA);
        subclass = st >> 4 & 0xf;
        lumClass = Lum_Unknown;
        break;
    case NeutronStar:
        if ((st >> 4 & 0xf) >= NeutronStarClassCount)
            return false;
        specClass = static_cast<SpectralClass>((st >> 4 & 0xf) + SpectralClass::Spectral_Q);
        subclass = st >> 4 & 0xf;
        lumClass = Lum_Unknown;
        break;
    case BlackHole:
        specClass = Spectral_Unknown;
        subclass = Subclass_Unknown;
        lumClass = Lum_Unknown;
        break;
    default:
        return false;
    }

    return true;
}


bool
StellarClass::unpackV2(uint16_t st)
{
    starType = static_cast<StellarClass::StarType>(st >> 13);

    switch (starType)
    {
    case NormalStar:
        specClass = static_cast<SpectralClass>(st >> 8 & 0x1f);
        subclass = st >> 4 & 0xf;
        lumClass = static_cast<LuminosityClass>(st & 0xf);
        break;
    case WhiteDwarf:
        if ((st >> 8 & 0xf) >= WDClassCount)
            return false;
        specClass = static_cast<SpectralClass>((st >> 8 & 0xf) + SpectralClass::Spectral_DA);
        subclass = st >> 4 & 0xf;
        lumClass = Lum_Unknown;
        break;
    case NeutronStar:
        if ((st >> 4 & 0xf) >= NeutronStarClassCount)
            return false;
        specClass = static_cast<SpectralClass>((st >> 4 & 0xf) + SpectralClass::Spectral_Q);
        subclass = st >> 4 & 0xf;
        lumClass = Lum_Unknown;
        break;
    case BlackHole:
        specClass = Spectral_Unknown;
        subclass = Subclass_Unknown;
        lumClass = Lum_Unknown;
        break;
    default:
        return false;
    }

    return true;
}


ostream& operator<<(ostream& os, const StellarClass& sc)
{
    os << sc.str();

    return os;
}


bool operator<(const StellarClass& sc0, const StellarClass& sc1)
{
    return sc0.packV2() < sc1.packV2();
}


// The following code implements a state machine for parsing spectral
// types.  It is a very forgiving parser, returning unknown for any of the
// spectral type fields it can't find, and silently ignoring any extra
// characters in the spectral type.  The parser is written this way because
// the spectral type strings from the Hipparcos catalog are quite irregular.
enum ParseState
{
    BeginState,
    EndState,
    NormalStarState,
    WolfRayetTypeState,
    NormalStarClassState,
    NormalStarSubclassState,
    NormalStarSubclassDecimalState,
    NormalStarSubclassFinalState,
    LumClassBeginState,
    LumClassIState,
    LumClassIIState,
    LumClassVState,
    LumClassIdashState,
    LumClassIaState,
    WDTypeState,
    WDExtendedTypeState,
    WDSubclassState,
    NeutronStarTypeState,
    NeutronStarExtendedTypeState,
    NeutronStarSubclassState,
    SubdwarfPrefixState,
};


StellarClass
StellarClass::parse(const string& st)
{
    uint32_t i = 0;
    ParseState state = BeginState;
    StellarClass::StarType starType = StellarClass::NormalStar;
    StellarClass::SpectralClass specClass = StellarClass::Spectral_Unknown;
    StellarClass::LuminosityClass lumClass = StellarClass::Lum_Unknown;
    unsigned int subclass = StellarClass::Subclass_Unknown;

    while (state != EndState)
    {
        char c;
        if (i < st.length())
            c = st[i];
        else
            c = '\0';

        switch (state)
        {
        case BeginState:
            switch (c)
            {
            case 'Q':
                starType = StellarClass::NeutronStar;
                specClass = StellarClass::Spectral_Q;
                state = NeutronStarTypeState;
                break;
            case 'X':
                starType = StellarClass::BlackHole;
                state = EndState;
                break;
            case 'D':
                starType = StellarClass::WhiteDwarf;
                specClass = StellarClass::Spectral_D;
                state = WDTypeState;
                i++;
                break;
            case 's':
                // Hipparcos uses sd prefix for stars with luminosity
                // class VI ('subdwarfs')
                state = SubdwarfPrefixState;
                i++;
                break;
            case '?':
                state = EndState;
                break;
            default:
                state = NormalStarClassState;
                break;
            }
            break;

        case WolfRayetTypeState:
            switch (c)
            {
            case 'C':
                specClass = StellarClass::Spectral_WC;
                state = NormalStarSubclassState;
                i++;
                break;
            case 'N':
                specClass = StellarClass::Spectral_WN;
                state = NormalStarSubclassState;
                i++;
                break;
            case 'O':
                specClass = StellarClass::Spectral_WO;
                state = NormalStarSubclassState;
                i++;
                break;
            default:
                specClass = StellarClass::Spectral_WC;
                state = NormalStarSubclassState;
                break;
            }
            break;

        case SubdwarfPrefixState:
            if (c == 'd')
            {
                lumClass = StellarClass::Lum_VI;
                state = NormalStarClassState;
                i++;
                break;
            }
            else
            {
                state = EndState;
            }
            break;

        case NormalStarClassState:
            switch (c)
            {
            case 'W':
                state = WolfRayetTypeState;
                break;
            case 'O':
                specClass = StellarClass::Spectral_O;
                state = NormalStarSubclassState;
                break;
            case 'B':
                specClass = StellarClass::Spectral_B;
                state = NormalStarSubclassState;
                break;
            case 'A':
                specClass = StellarClass::Spectral_A;
                state = NormalStarSubclassState;
                break;
            case 'F':
                specClass = StellarClass::Spectral_F;
                state = NormalStarSubclassState;
                break;
            case 'G':
                specClass = StellarClass::Spectral_G;
                state = NormalStarSubclassState;
                break;
            case 'K':
                specClass = StellarClass::Spectral_K;
                state = NormalStarSubclassState;
                break;
            case 'M':
                specClass = StellarClass::Spectral_M;
                state = NormalStarSubclassState;
                break;
            case 'R':
                specClass = StellarClass::Spectral_R;
                state = NormalStarSubclassState;
                break;
            case 'S':
                specClass = StellarClass::Spectral_S;
                state = NormalStarSubclassState;
                break;
            case 'N':
                specClass = StellarClass::Spectral_N;
                state = NormalStarSubclassState;
                break;
            case 'L':
                specClass = StellarClass::Spectral_L;
                state = NormalStarSubclassState;
                break;
            case 'T':
                specClass = StellarClass::Spectral_T;
                state = NormalStarSubclassState;
                break;
            case 'Y':
                specClass = StellarClass::Spectral_Y;
                state = NormalStarSubclassState;
                break;
            case 'C':
                specClass = StellarClass::Spectral_C;
                state = NormalStarSubclassState;
                break;
            default:
                state = EndState;
                break;
            }
            i++;
            break;

        case NormalStarSubclassState:
            if (isdigit(c))
            {
                subclass = (unsigned int) c - (unsigned int) '0';
                state = NormalStarSubclassDecimalState;
                i++;
            }
            else
            {
                state = LumClassBeginState;
            }
            break;

        case NormalStarSubclassDecimalState:
            if (c == '.')
            {
                state = NormalStarSubclassFinalState;
                i++;
            }
            else
            {
                state = LumClassBeginState;
            }
            break;

        case NormalStarSubclassFinalState:
            if (isdigit(c))
                state = LumClassBeginState;
            else
                state = EndState;
            i++;
            break;

        case LumClassBeginState:
            switch (c)
            {
            case 'I':
                state = LumClassIState;
                break;
            case 'V':
                state = LumClassVState;
                break;
            default:
                state = EndState;
                break;
            }
            i++;
            break;

        case LumClassIState:
            switch (c)
            {
            case 'I':
                state = LumClassIIState;
                break;
            case 'V':
                lumClass = StellarClass::Lum_IV;
                state = EndState;
                break;
            case 'a':
                state = LumClassIaState;
                break;
            case 'b':
                lumClass = StellarClass::Lum_Ib;
                state = EndState;
                break;
            case '-':
                state = LumClassIdashState;
                break;
            default:
                lumClass = StellarClass::Lum_Ib;
                state = EndState;
                break;
            }
            i++;
            break;

        case LumClassIIState:
            switch (c)
            {
            case 'I':
                lumClass = StellarClass::Lum_III;
                state = EndState;
                break;
            default:
                lumClass = StellarClass::Lum_II;
                state = EndState;
                break;
            }
            break;

        case LumClassIdashState:
            switch (c)
            {
            case 'a':
                state = LumClassIaState;
                break;
            case 'b':
                lumClass = StellarClass::Lum_Ib;
                state = EndState;
                break;
            default:
                lumClass = StellarClass::Lum_Ib;
                state = EndState;
                break;
            }
            break;

        case LumClassIaState:
            switch (c)
            {
            case '0':
                lumClass = StellarClass::Lum_Ia0;
                state = EndState;
                break;
            default:
                lumClass = StellarClass::Lum_Ia;
                state = EndState;
                break;
            }
            break;

        case LumClassVState:
            switch (c)
            {
            case 'I':
                lumClass = StellarClass::Lum_VI;
                state = EndState;
                break;
            default:
                lumClass = StellarClass::Lum_V;
                state = EndState;
                break;
            }
            break;

        case WDTypeState:
            switch (c)
            {
            case 'A':
                specClass = StellarClass::Spectral_DA;
                i++;
                break;
            case 'B':
                specClass = StellarClass::Spectral_DB;
                i++;
                break;
            case 'C':
                specClass = StellarClass::Spectral_DC;
                i++;
                break;
            case 'O':
                specClass = StellarClass::Spectral_DO;
                i++;
                break;
            case 'Q':
                specClass = StellarClass::Spectral_DQ;
                i++;
                break;
            case 'X':
                specClass = StellarClass::Spectral_DX;
                i++;
                break;
            case 'Z':
                specClass = StellarClass::Spectral_DZ;
                i++;
                break;
            default:
                specClass = StellarClass::Spectral_D;
                break;
            }
            state = WDExtendedTypeState;
            break;

        case WDExtendedTypeState:
            switch (c)
            {
            case 'A':
            case 'B':
            case 'C':
            case 'O':
            case 'Q':
            case 'Z':
            case 'X':
            case 'V': // variable
            case 'P': // magnetic stars with polarized light
            case 'H': // magnetic stars without polarized light
            case 'E': // emission lines
                i++;
                break;
            default:
                state = WDSubclassState;
                break;
            }
            break;

        case WDSubclassState:
            if (isdigit(c))
            {
                subclass = (unsigned int) c - (unsigned int) '0';
                i++;
            }
            state = EndState;
            break;

        case NeutronStarTypeState:
            switch (c)
            {
            case 'N':
                specClass = StellarClass::Spectral_QN;
                i++;
                break;
            case 'P':
                specClass = StellarClass::Spectral_QP;
                i++;
                break;
            case 'M':
                specClass = StellarClass::Spectral_QM;
                i++;
                break;
            default:
                specClass = StellarClass::Spectral_Q;
                break;
            }
            state = NeutronStarExtendedTypeState;
            break;

        case NeutronStarExtendedTypeState:
            switch (c)
            {
            case 'P': // pulsars
            case 'M': // magnetars
            case 'N': // non-pulsating neutron stars
                i++;
                break;
            default:
                state = NeutronStarSubclassState;
                break;
            }
            break;

        case NeutronStarSubclassState:
            if (isdigit(c))
            {
                subclass = (unsigned int) c - (unsigned int) '0';
                i++;
            }
            state = EndState;
            break;

        default:
            assert(0);
            state = EndState;
            break;
        }
    }

    return {starType, specClass, subclass, lumClass};
}
