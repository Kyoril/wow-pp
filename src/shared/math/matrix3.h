//
// This file is part of the WoW++ project.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Genral Public License as published by
// the Free Software Foudnation; either version 2 of the Licanse, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
//

#pragma once

#include "vector3.h"
#include <cassert>
#include <utility>
#include <cmath>
#include <cstddef>

namespace wowpp
{
	namespace math
	{
		struct Matrix3 final
		{
			static const Matrix3 ZERO;
			static const Matrix3 IDENTITY;

			/// The matrix entries, indexed by [row][col].
			union {
				float m[3][3];
				float m_[9];
			};

			inline Matrix3()
			{
				identity();
			}
			inline Matrix3(
			    float m00, float m01, float m02,
			    float m10, float m11, float m12,
			    float m20, float m21, float m22)
			{
				m[0][0] = m00;
				m[0][1] = m01;
				m[0][2] = m02;
				m[1][0] = m10;
				m[1][1] = m11;
				m[1][2] = m12;
				m[2][0] = m20;
				m[2][1] = m21;
				m[2][2] = m22;
			}
			void identity()
			{
				m[0][0] = 1.0f;
				m[0][1] = 0.0f;
				m[0][2] = 0.0f;
				m[1][0] = 0.0f;
				m[1][1] = 1.0f;
				m[1][2] = 0.0f;
				m[2][0] = 0.0f;
				m[2][1] = 0.0f;
				m[2][2] = 1.0f;
			}
			inline void swap(Matrix3 &other)
			{
				std::swap(m[0][0], other.m[0][0]);
				std::swap(m[0][1], other.m[0][1]);
				std::swap(m[0][2], other.m[0][2]);
				std::swap(m[1][0], other.m[1][0]);
				std::swap(m[1][1], other.m[1][1]);
				std::swap(m[1][2], other.m[1][2]);
				std::swap(m[2][0], other.m[2][0]);
				std::swap(m[2][1], other.m[2][1]);
				std::swap(m[2][2], other.m[2][2]);
			}

			inline float *operator [] (std::size_t iRow)
			{
				assert(iRow < 3);
				return m[iRow];
			}

			inline const float *operator [] (std::size_t iRow) const
			{
				assert(iRow < 3);
				return m[iRow];
			}
						
			static Matrix3 fromEulerAnglesXYZ(float fYAngle, float fPAngle, float fRAngle)
			{
				float fCos, fSin;

				fCos = cosf(fYAngle);
				fSin = sinf(fYAngle);
				Matrix3 kXMat(1.0f, 0.0f, 0.0f,
				              0.0f, fCos, -fSin,
				              0.0, fSin, fCos);

				fCos = cosf(fPAngle);
				fSin = sinf(fPAngle);
				Matrix3 kYMat(fCos, 0.0f, fSin,
				              0.0f, 1.0f, 0.0f,
				              -fSin, 0.0f, fCos);

				fCos = cosf(fRAngle);
				fSin = sinf(fRAngle);
				Matrix3 kZMat(fCos, -fSin, 0.0f,
				              fSin, fCos, 0.0f,
				              0.0f, 0.0f, 1.0f);

				return kZMat * (kYMat * kXMat);
			}

			void fromAngleAxis(const Vector3 &rkAxis, float fRadians)
			{
				float fCos = ::cosf(fRadians);
				float fSin = ::sinf(fRadians);
				float fOneMinusCos = 1.0f - fCos;
				float fX2 = rkAxis.x * rkAxis.x;
				float fY2 = rkAxis.y * rkAxis.y;
				float fZ2 = rkAxis.z * rkAxis.z;
				float fXYM = rkAxis.x * rkAxis.y * fOneMinusCos;
				float fXZM = rkAxis.x * rkAxis.z * fOneMinusCos;
				float fYZM = rkAxis.y * rkAxis.z * fOneMinusCos;
				float fXSin = rkAxis.x * fSin;
				float fYSin = rkAxis.y * fSin;
				float fZSin = rkAxis.z * fSin;

				m[0][0] = fX2 * fOneMinusCos + fCos;
				m[0][1] = fXYM - fZSin;
				m[0][2] = fXZM + fYSin;
				m[1][0] = fXYM + fZSin;
				m[1][1] = fY2 * fOneMinusCos + fCos;
				m[1][2] = fYZM - fXSin;
				m[2][0] = fXZM - fYSin;
				m[2][1] = fYZM + fXSin;
				m[2][2] = fZ2 * fOneMinusCos + fCos;
			}


			inline Matrix3 operator * (const Matrix3 &m2) const
			{
				Matrix3 kProd;
				for (size_t iRow = 0; iRow < 3; iRow++)
				{
					for (size_t iCol = 0; iCol < 3; iCol++)
					{
						kProd.m[iRow][iCol] =
							m[iRow][0] * m2.m[0][iCol] +
							m[iRow][1] * m2.m[1][iCol] +
							m[iRow][2] * m2.m[2][iCol];
					}
				}
				return kProd;
			}

			inline Vector3 operator * (const Vector3 &v) const
			{
				Vector3 kProd;
				for (size_t iRow = 0; iRow < 3; iRow++)
				{
					kProd[iRow] =
						m[iRow][0] * v[0] +
						m[iRow][1] * v[1] +
						m[iRow][2] * v[2];
				}
				return kProd;
			}
		};
	}
}
