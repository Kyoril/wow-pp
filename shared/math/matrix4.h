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

namespace wowpp
{
	namespace math
	{
		/// Represents a 4x4 matrix, which is used to transform positions. Mainly used
		/// to transform from one of wow's coordinate systems to another coordinate system
		/// which for some reason is used in wow, too.
		/// 
		///     [ m[0][0]  m[0][1]  m[0][2]  m[0][3] ]   {x}
		///		| m[1][0]  m[1][1]  m[1][2]  m[1][3] | * {y}
		///		| m[2][0]  m[2][1]  m[2][2]  m[2][3] |   {z}
		///		[ m[3][0]  m[3][1]  m[3][2]  m[3][3] ]   {1}
		struct Matrix4 final
		{
			static const Matrix4 ZERO;
			static const Matrix4 IDENTITY;

			/// The matrix entries, indexed by [row][col].
			union {
				float m[4][4];
				float m_[16];
			};

			inline Matrix4()
			{
			}
			inline Matrix4(
				float m00, float m01, float m02, float m03,
				float m10, float m11, float m12, float m13,
				float m20, float m21, float m22, float m23,
				float m30, float m31, float m32, float m33)
			{
				m[0][0] = m00;
				m[0][1] = m01;
				m[0][2] = m02;
				m[0][3] = m03;
				m[1][0] = m10;
				m[1][1] = m11;
				m[1][2] = m12;
				m[1][3] = m13;
				m[2][0] = m20;
				m[2][1] = m21;
				m[2][2] = m22;
				m[2][3] = m23;
				m[3][0] = m30;
				m[3][1] = m31;
				m[3][2] = m32;
				m[3][3] = m33;
			}

			inline void swap(Matrix4& other)
			{
				std::swap(m[0][0], other.m[0][0]);
				std::swap(m[0][1], other.m[0][1]);
				std::swap(m[0][2], other.m[0][2]);
				std::swap(m[0][3], other.m[0][3]);
				std::swap(m[1][0], other.m[1][0]);
				std::swap(m[1][1], other.m[1][1]);
				std::swap(m[1][2], other.m[1][2]);
				std::swap(m[1][3], other.m[1][3]);
				std::swap(m[2][0], other.m[2][0]);
				std::swap(m[2][1], other.m[2][1]);
				std::swap(m[2][2], other.m[2][2]);
				std::swap(m[2][3], other.m[2][3]);
				std::swap(m[3][0], other.m[3][0]);
				std::swap(m[3][1], other.m[3][1]);
				std::swap(m[3][2], other.m[3][2]);
				std::swap(m[3][3], other.m[3][3]);
			}

			inline float* operator [] (size_t iRow)
			{
				assert(iRow < 4);
				return m[iRow];
			}

			inline const float *operator [] (size_t iRow) const
			{
				assert(iRow < 4);
				return m[iRow];
			}

			inline Matrix4 concatenate(const Matrix4 &m2) const
			{
				Matrix4 r;
				r.m[0][0] = m[0][0] * m2.m[0][0] + m[0][1] * m2.m[1][0] + m[0][2] * m2.m[2][0] + m[0][3] * m2.m[3][0];
				r.m[0][1] = m[0][0] * m2.m[0][1] + m[0][1] * m2.m[1][1] + m[0][2] * m2.m[2][1] + m[0][3] * m2.m[3][1];
				r.m[0][2] = m[0][0] * m2.m[0][2] + m[0][1] * m2.m[1][2] + m[0][2] * m2.m[2][2] + m[0][3] * m2.m[3][2];
				r.m[0][3] = m[0][0] * m2.m[0][3] + m[0][1] * m2.m[1][3] + m[0][2] * m2.m[2][3] + m[0][3] * m2.m[3][3];

				r.m[1][0] = m[1][0] * m2.m[0][0] + m[1][1] * m2.m[1][0] + m[1][2] * m2.m[2][0] + m[1][3] * m2.m[3][0];
				r.m[1][1] = m[1][0] * m2.m[0][1] + m[1][1] * m2.m[1][1] + m[1][2] * m2.m[2][1] + m[1][3] * m2.m[3][1];
				r.m[1][2] = m[1][0] * m2.m[0][2] + m[1][1] * m2.m[1][2] + m[1][2] * m2.m[2][2] + m[1][3] * m2.m[3][2];
				r.m[1][3] = m[1][0] * m2.m[0][3] + m[1][1] * m2.m[1][3] + m[1][2] * m2.m[2][3] + m[1][3] * m2.m[3][3];

				r.m[2][0] = m[2][0] * m2.m[0][0] + m[2][1] * m2.m[1][0] + m[2][2] * m2.m[2][0] + m[2][3] * m2.m[3][0];
				r.m[2][1] = m[2][0] * m2.m[0][1] + m[2][1] * m2.m[1][1] + m[2][2] * m2.m[2][1] + m[2][3] * m2.m[3][1];
				r.m[2][2] = m[2][0] * m2.m[0][2] + m[2][1] * m2.m[1][2] + m[2][2] * m2.m[2][2] + m[2][3] * m2.m[3][2];
				r.m[2][3] = m[2][0] * m2.m[0][3] + m[2][1] * m2.m[1][3] + m[2][2] * m2.m[2][3] + m[2][3] * m2.m[3][3];

				r.m[3][0] = m[3][0] * m2.m[0][0] + m[3][1] * m2.m[1][0] + m[3][2] * m2.m[2][0] + m[3][3] * m2.m[3][0];
				r.m[3][1] = m[3][0] * m2.m[0][1] + m[3][1] * m2.m[1][1] + m[3][2] * m2.m[2][1] + m[3][3] * m2.m[3][1];
				r.m[3][2] = m[3][0] * m2.m[0][2] + m[3][1] * m2.m[1][2] + m[3][2] * m2.m[2][2] + m[3][3] * m2.m[3][2];
				r.m[3][3] = m[3][0] * m2.m[0][3] + m[3][1] * m2.m[1][3] + m[3][2] * m2.m[2][3] + m[3][3] * m2.m[3][3];

				return r;
			}

			inline Matrix4 transpose() const
			{
				return Matrix4(m[0][0], m[1][0], m[2][0], m[3][0],
					m[0][1], m[1][1], m[2][1], m[3][1],
					m[0][2], m[1][2], m[2][2], m[3][2],
					m[0][3], m[1][3], m[2][3], m[3][3]);
			}

			inline void setTranslation(const Vector3& v)
			{
				m[0][3] = v.x;
				m[1][3] = v.y;
				m[2][3] = v.z;
			}

			inline Vector3 getTranslation() const
			{
				return Vector3(m[0][3], m[1][3], m[2][3]);
			}

			inline void makeTranslation(const Vector3& v)
			{
				m[0][0] = 1.0; m[0][1] = 0.0; m[0][2] = 0.0; m[0][3] = v.x;
				m[1][0] = 0.0; m[1][1] = 1.0; m[1][2] = 0.0; m[1][3] = v.y;
				m[2][0] = 0.0; m[2][1] = 0.0; m[2][2] = 1.0; m[2][3] = v.z;
				m[3][0] = 0.0; m[3][1] = 0.0; m[3][2] = 0.0; m[3][3] = 1.0;
			}

			inline void makeTranslation(float tx, float ty, float tz)
			{
				m[0][0] = 1.0; m[0][1] = 0.0; m[0][2] = 0.0; m[0][3] = tx;
				m[1][0] = 0.0; m[1][1] = 1.0; m[1][2] = 0.0; m[1][3] = ty;
				m[2][0] = 0.0; m[2][1] = 0.0; m[2][2] = 1.0; m[2][3] = tz;
				m[3][0] = 0.0; m[3][1] = 0.0; m[3][2] = 0.0; m[3][3] = 1.0;
			}

			inline static Matrix4 getTranslation(const Vector3& v)
			{
				Matrix4 r;

				r.m[0][0] = 1.0; r.m[0][1] = 0.0; r.m[0][2] = 0.0; r.m[0][3] = v.x;
				r.m[1][0] = 0.0; r.m[1][1] = 1.0; r.m[1][2] = 0.0; r.m[1][3] = v.y;
				r.m[2][0] = 0.0; r.m[2][1] = 0.0; r.m[2][2] = 1.0; r.m[2][3] = v.z;
				r.m[3][0] = 0.0; r.m[3][1] = 0.0; r.m[3][2] = 0.0; r.m[3][3] = 1.0;

				return r;
			}

			inline static Matrix4 getTranslation(float t_x, float t_y, float t_z)
			{
				Matrix4 r;

				r.m[0][0] = 1.0; r.m[0][1] = 0.0; r.m[0][2] = 0.0; r.m[0][3] = t_x;
				r.m[1][0] = 0.0; r.m[1][1] = 1.0; r.m[1][2] = 0.0; r.m[1][3] = t_y;
				r.m[2][0] = 0.0; r.m[2][1] = 0.0; r.m[2][2] = 1.0; r.m[2][3] = t_z;
				r.m[3][0] = 0.0; r.m[3][1] = 0.0; r.m[3][2] = 0.0; r.m[3][3] = 1.0;

				return r;
			}

			inline void setScale(const Vector3& v)
			{
				m[0][0] = v.x;
				m[1][1] = v.y;
				m[2][2] = v.z;
			}

			inline static Matrix4 getScale(const Vector3& v)
			{
				Matrix4 r;
				r.m[0][0] = v.x; r.m[0][1] = 0.0; r.m[0][2] = 0.0; r.m[0][3] = 0.0;
				r.m[1][0] = 0.0; r.m[1][1] = v.y; r.m[1][2] = 0.0; r.m[1][3] = 0.0;
				r.m[2][0] = 0.0; r.m[2][1] = 0.0; r.m[2][2] = v.z; r.m[2][3] = 0.0;
				r.m[3][0] = 0.0; r.m[3][1] = 0.0; r.m[3][2] = 0.0; r.m[3][3] = 1.0;

				return r;
			}

			inline static Matrix4 getScale(float s_x, float s_y, float s_z)
			{
				Matrix4 r;
				r.m[0][0] = s_x; r.m[0][1] = 0.0; r.m[0][2] = 0.0; r.m[0][3] = 0.0;
				r.m[1][0] = 0.0; r.m[1][1] = s_y; r.m[1][2] = 0.0; r.m[1][3] = 0.0;
				r.m[2][0] = 0.0; r.m[2][1] = 0.0; r.m[2][2] = s_z; r.m[2][3] = 0.0;
				r.m[3][0] = 0.0; r.m[3][1] = 0.0; r.m[3][2] = 0.0; r.m[3][3] = 1.0;

				return r;
			}

			void fromAngleAxis(const Vector3& rkAxis, float fRadians)
			{
				float fCos = ::cosf(fRadians);
				float fSin = ::sinf(fRadians);
				float fOneMinusCos = 1.0f - fCos;
				float fX2 = rkAxis.x*rkAxis.x;
				float fY2 = rkAxis.y*rkAxis.y;
				float fZ2 = rkAxis.z*rkAxis.z;
				float fXYM = rkAxis.x*rkAxis.y*fOneMinusCos;
				float fXZM = rkAxis.x*rkAxis.z*fOneMinusCos;
				float fYZM = rkAxis.y*rkAxis.z*fOneMinusCos;
				float fXSin = rkAxis.x*fSin;
				float fYSin = rkAxis.y*fSin;
				float fZSin = rkAxis.z*fSin;

				m[0][0] = fX2*fOneMinusCos + fCos;
				m[0][1] = fXYM - fZSin;
				m[0][2] = fXZM + fYSin;
				m[1][0] = fXYM + fZSin;
				m[1][1] = fY2*fOneMinusCos + fCos;
				m[1][2] = fYZM - fXSin;
				m[2][0] = fXZM - fYSin;
				m[2][1] = fYZM + fXSin;
				m[2][2] = fZ2*fOneMinusCos + fCos;
			}


			inline Matrix4 operator * (const Matrix4 &m2) const
			{
				return concatenate(m2);
			}

			inline Vector3 operator * (const Vector3 &v) const
			{
				Vector3 r;

				float fInvW = 1.0f / (m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3]);

				r.x = (m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3]) * fInvW;
				r.y = (m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3]) * fInvW;
				r.z = (m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3]) * fInvW;

				return r;
			}

			inline Matrix4 operator + (const Matrix4 &m2) const
			{
				Matrix4 r;

				r.m[0][0] = m[0][0] + m2.m[0][0];
				r.m[0][1] = m[0][1] + m2.m[0][1];
				r.m[0][2] = m[0][2] + m2.m[0][2];
				r.m[0][3] = m[0][3] + m2.m[0][3];

				r.m[1][0] = m[1][0] + m2.m[1][0];
				r.m[1][1] = m[1][1] + m2.m[1][1];
				r.m[1][2] = m[1][2] + m2.m[1][2];
				r.m[1][3] = m[1][3] + m2.m[1][3];

				r.m[2][0] = m[2][0] + m2.m[2][0];
				r.m[2][1] = m[2][1] + m2.m[2][1];
				r.m[2][2] = m[2][2] + m2.m[2][2];
				r.m[2][3] = m[2][3] + m2.m[2][3];

				r.m[3][0] = m[3][0] + m2.m[3][0];
				r.m[3][1] = m[3][1] + m2.m[3][1];
				r.m[3][2] = m[3][2] + m2.m[3][2];
				r.m[3][3] = m[3][3] + m2.m[3][3];

				return r;
			}

			inline Matrix4 operator - (const Matrix4 &m2) const
			{
				Matrix4 r;
				r.m[0][0] = m[0][0] - m2.m[0][0];
				r.m[0][1] = m[0][1] - m2.m[0][1];
				r.m[0][2] = m[0][2] - m2.m[0][2];
				r.m[0][3] = m[0][3] - m2.m[0][3];

				r.m[1][0] = m[1][0] - m2.m[1][0];
				r.m[1][1] = m[1][1] - m2.m[1][1];
				r.m[1][2] = m[1][2] - m2.m[1][2];
				r.m[1][3] = m[1][3] - m2.m[1][3];

				r.m[2][0] = m[2][0] - m2.m[2][0];
				r.m[2][1] = m[2][1] - m2.m[2][1];
				r.m[2][2] = m[2][2] - m2.m[2][2];
				r.m[2][3] = m[2][3] - m2.m[2][3];

				r.m[3][0] = m[3][0] - m2.m[3][0];
				r.m[3][1] = m[3][1] - m2.m[3][1];
				r.m[3][2] = m[3][2] - m2.m[3][2];
				r.m[3][3] = m[3][3] - m2.m[3][3];

				return r;
			}

			inline bool operator == (const Matrix4& m2) const
			{
				return !(
					m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
					m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
					m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
					m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3]);
			}

			inline bool operator != (const Matrix4& m2) const
			{
				return (
					m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
					m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
					m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
					m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3]);
			}
		};
	}
}
