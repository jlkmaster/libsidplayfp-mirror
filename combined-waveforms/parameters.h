/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2013-2016 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <cmath>

#include <vector>
#include <string>
#include <sstream>

// Model parameters
enum class Param_t
{
    THRESHOLD,
    PULSESTRENGTH,
    TOPBIT,
    DISTANCE,
    STMIX
};
// define the postfix increment operator to allow looping over enum
inline Param_t& operator++(Param_t& x, int)
{
    return x = static_cast<Param_t>(static_cast<std::underlying_type<Param_t>::type>(x) + 1);
}

typedef std::vector<unsigned int> ref_vector_t;

struct score_t
{
    unsigned int audible_error;
    unsigned int wrong_bits;

    score_t() :
        audible_error(0),
        wrong_bits(0)
    {}

    double wrongBitsRate() const
    {
        return static_cast<double>(wrong_bits*1000)/(4096*8);
    }

    bool isBetter(const score_t& newScore) const
    {
        return (newScore.audible_error < audible_error)
            || ((newScore.audible_error == audible_error)
                && (newScore.wrong_bits < wrong_bits));
    }
};

std::ostream & operator<<(std::ostream & os, const score_t & foo)
{
   os.precision(2);
   os << foo.audible_error << " (" << std::fixed << foo.wrongBitsRate() << ")";
   return os;
}

class Parameters
{
public:
    float threshold, pulsestrength, topbit, distance, stmix;

public:
    Parameters() { reset(); }

    void reset()
    {
        threshold = 0.f;
        pulsestrength = 0.f;
        topbit = 0.f;
        distance = 0.f;
        stmix = 0.f;
    }

    float GetValue(Param_t i)
    {
        switch (i)
        {
            case Param_t::THRESHOLD: return threshold;
            case Param_t::PULSESTRENGTH: return pulsestrength;
            case Param_t::TOPBIT: return topbit;
            case Param_t::DISTANCE: return distance;
            case Param_t::STMIX: return stmix;
        }
    }

    void SetValue(Param_t i, float v)
    {
        switch (i)
        {
            case Param_t::THRESHOLD: threshold = v; break;
            case Param_t::PULSESTRENGTH: pulsestrength = v; break;
            case Param_t::TOPBIT: topbit = v; break;
            case Param_t::DISTANCE: distance = v; break;
            case Param_t::STMIX: stmix = v; break;
        }
    }

    std::string toString()
    {
        std::ostringstream ss;
        ss << "threshold = " << threshold << std::endl;
        ss << "pulsestrength = " << pulsestrength << std::endl;
        ss << "topbit = " << topbit << std::endl;
        ss << "distance = " << distance << std::endl;
        ss << "stmix = " << stmix << std::endl;
        return ss.str();
    }

private:
    void SimulateMix(float bitarray[12], float wa[], bool HasPulse) const
    {
        float tmp[12];

        for (int sb = 0; sb < 12; sb++)
        {
            float n = 0.f;
            float avg = 0.f;
            for (int cb = 0; cb < 12; cb++)
            {
                const float weight = wa[sb - cb + 12];
                avg += bitarray[cb] * weight;
                n += weight;
            }
            if (HasPulse)
            {
                const float weight = wa[sb];
                avg += pulsestrength * weight;
                n += weight;
            }
            tmp[sb] = (bitarray[sb] + avg / n) * 0.5f;
        }
        for (int i = 0; i < 12; i++)
            bitarray[i] = tmp[i];
    }

    /**
     * Get the upper 8 bits of the predicted value.
     */
    unsigned int GetScore8(float bitarray[12]) const
    {
        unsigned int result = 0;
        for (int cb = 0; cb < 8; cb++)
        {
            if (bitarray[4+cb] > threshold)
                result |= 1 << cb;
        }
        return result;
    }

    /**
     * Calculate audible error.
     */
    static unsigned int ScoreResult(unsigned int a, unsigned int b)
    {
        return a ^ b;
    }

    /**
     * Count number of mispredicted bits.
     */
    static unsigned int WrongBits(unsigned int v)
    {
        // Brian Kernighan's method goes through as many iterations as there are set bits
        unsigned int c = 0;
        for (; v; c++)
        {
          v &= v - 1;
        }
        return c;
    }

    float getAnalogValue(float bitarray[12]) const
    {
        float analogval = 0.f;
        for (unsigned int i = 0; i < 12; i++)
        {
            float val = (bitarray[i] - threshold) * 512 + 0.5f;
            if (val < 0.f)
                val = 0.f;
            else if (val > 1.f)
                val = 1.f;
            analogval += val * (1 << i);
        }
        return analogval / 16.f;
    }

public:
    score_t Score(int wave, const ref_vector_t &reference, bool print, unsigned int bestscore)
    {
        score_t score;

        /*
         * Calculate the weight as an exponential function of distance.
         * The quadratic model gives better results for some combinations
         * like waveforms 5 and 6 for 8580 model, maybe something worth looking into.
         */
        float wa[12 * 2 + 1];
        for (int i = 0; i <= 12; i++)
        {
            wa[12-i] = wa[12+i] = 1.0f / pow(distance, i); // (1.f + (i*i) * distance);
        }

        // loop over the 4096 oscillator values
        for (unsigned int j = 0; j < 4096; j++)
        {
            float bitarray[12];

            // Saw
            for (unsigned int i = 0; i < 12; i++)
            {
                bitarray[i] = (j & (1 << i)) != 0 ? 1.f : 0.f;
            }

            // Change to Triangle
            if ((wave & 3) == 1)
            {
                const bool top = (j & 2048) != 0;
                for (int i = 11; i > 0; i--)
                {
                    bitarray[i] = top ? 1.f - bitarray[i-1] : bitarray[i-1];
                }
                bitarray[0] = 0.f;
            }

            // or Saw + Triangle
            else if ((wave & 3) == 3)
            {
                bitarray[0] *= stmix;
                const float compl_stmix = 1.f - stmix;
                for (int i = 1; i < 12; i++)
                {
                    bitarray[i] = bitarray[i-1] * compl_stmix + bitarray[i] * stmix;
                }
            }

            // topbit for Saw
            if ((wave & 2) == 2)
            {
                bitarray[11] *= topbit;
            }

            SimulateMix(bitarray, wa, wave > 4);

            // Calculate score
            const unsigned int simval = GetScore8(bitarray);
            const unsigned int refval = reference[j];
            const unsigned int error = ScoreResult(simval, refval);
            score.audible_error += error;
            score.wrong_bits += WrongBits(error);

            if (print)
            {
                std::cout << j << " "
                          << refval << " "
                          << simval << " "
                          << (simval ^ refval) << " "
#if 0
                          << getAnalogValue(bitarray) << " "
#endif
                          << std::endl;
            }

            // halt if we already are worst than the best score
            if (score.audible_error > bestscore)
            {
                return score;
            }
        }
#if 0
        // print the rate of wrong bits
        std::cout << score.wrongBitsRate() << std::endl;
#endif
        return score;
    }
};

#endif
