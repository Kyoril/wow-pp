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

namespace io
{
	class Reader;
	class Writer;
}

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
				identity();
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
			void identity()
			{
				m[0][0] = 1.0f;
				m[0][1] = 0.0f;
				m[0][2] = 0.0f;
				m[0][3] = 0.0f;
				m[1][0] = 0.0f;
				m[1][1] = 1.0f;
				m[1][2] = 0.0f;
				m[1][3] = 0.0f;
				m[2][0] = 0.0f;
				m[2][1] = 0.0f;
				m[2][2] = 1.0f;
				m[2][3] = 0.0f;
				m[3][0] = 0.0f;
				m[3][1] = 0.0f;
				m[3][2] = 0.0f;
				m[3][3] = 1.0f;
			}
			inline void swap(Matrix4 &other)
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

			inline float *operator [] (std::size_t iRow)
			{
				assert(iRow < 4);
				return m[iRow];
			}

			inline const float *operator [] (std::size_t iRow) const
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

			inline void setTranslation(const Vector3 &v)
			{
				m[0][3] = v.x;
				m[1][3] = v.y;
				m[2][3] = v.z;
			}

			inline Vector3 getTranslation() const
			{
				return Vector3(m[0][3], m[1][3], m[2][3]);
			}

			inline void makeTranslation(const Vector3 &v)
			{
				m[0][0] = 1.0;
				m[0][1] = 0.0;
				m[0][2] = 0.0;
				m[0][3] = v.x;
				m[1][0] = 0.0;
				m[1][1] = 1.0;
				m[1][2] = 0.0;
				m[1][3] = v.y;
				m[2][0] = 0.0;
				m[2][1] = 0.0;
				m[2][2] = 1.0;
				m[2][3] = v.z;
				m[3][0] = 0.0;
				m[3][1] = 0.0;
				m[3][2] = 0.0;
				m[3][3] = 1.0;
			}

			inline void makeTranslation(float tx, float ty, float tz)
			{
				m[0][0] = 1.0;
				m[0][1] = 0.0;
				m[0][2] = 0.0;
				m[0][3] = tx;
				m[1][0] = 0.0;
				m[1][1] = 1.0;
				m[1][2] = 0.0;
				m[1][3] = ty;
				m[2][0] = 0.0;
				m[2][1] = 0.0;
				m[2][2] = 1.0;
				m[2][3] = tz;
				m[3][0] = 0.0;
				m[3][1] = 0.0;
				m[3][2] = 0.0;
				m[3][3] = 1.0;
			}

			inline static Matrix4 getTranslation(const Vector3 &v)
			{
				return getTranslation(v.x, v.y, v.z);
			}

			inline static Matrix4 getTranslation(float t_x, float t_y, float t_z)
			{
				Matrix4 r;

				for (int y = 0; y < 4; ++y)
					for (int x = 0; x < 4; ++x)
						r.m[y][x] = 0.f;

				r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
				r.m[0][3] = t_x;
				r.m[1][3] = t_y;
				r.m[2][3] = t_z;

				return r;
			}

			inline void setScale(const Vector3 &v)
			{
				m[0][0] = v.x;
				m[1][1] = v.y;
				m[2][2] = v.z;
			}

			inline static Matrix4 getScale(const Vector3 &v)
			{
				Matrix4 r;
				r.m[0][0] = v.x;
				r.m[0][1] = 0.0;
				r.m[0][2] = 0.0;
				r.m[0][3] = 0.0;
				r.m[1][0] = 0.0;
				r.m[1][1] = v.y;
				r.m[1][2] = 0.0;
				r.m[1][3] = 0.0;
				r.m[2][0] = 0.0;
				r.m[2][1] = 0.0;
				r.m[2][2] = v.z;
				r.m[2][3] = 0.0;
				r.m[3][0] = 0.0;
				r.m[3][1] = 0.0;
				r.m[3][2] = 0.0;
				r.m[3][3] = 1.0;

				return r;
			}

			Matrix4 inverse() const;

			inline static Matrix4 getScale(float s_x, float s_y, float s_z)
			{
				Matrix4 r;
				r.m[0][0] = s_x;
				r.m[0][1] = 0.0;
				r.m[0][2] = 0.0;
				r.m[0][3] = 0.0;
				r.m[1][0] = 0.0;
				r.m[1][1] = s_y;
				r.m[1][2] = 0.0;
				r.m[1][3] = 0.0;
				r.m[2][0] = 0.0;
				r.m[2][1] = 0.0;
				r.m[2][2] = s_z;
				r.m[2][3] = 0.0;
				r.m[3][0] = 0.0;
				r.m[3][1] = 0.0;
				r.m[3][2] = 0.0;
				r.m[3][3] = 1.0;

				return r;
			}

			static Matrix4 fromEulerAnglesXYZ(float fYAngle, float fPAngle, float fRAngle)
			{
				float fCos, fSin;

				fCos = cosf(fYAngle);
				fSin = sinf(fYAngle);
				Matrix4 kXMat(1.0f, 0.0f, 0.0f, 0.0f,
				              0.0f, fCos, -fSin, 0.0f,
				              0.0, fSin, fCos, 0.0f,
				              0.0f, 0.0f, 0.0f, 1.0f);

				fCos = cosf(fPAngle);
				fSin = sinf(fPAngle);
				Matrix4 kYMat(fCos, 0.0f, fSin, 0.0f,
				              0.0f, 1.0f, 0.0f, 0.0f,
				              -fSin, 0.0f, fCos, 0.0f,
				              0.0f, 0.0f, 0.0f, 1.0f);

				fCos = cosf(fRAngle);
				fSin = sinf(fRAngle);
				Matrix4 kZMat(fCos, -fSin, 0.0f, 0.0f,
				              fSin, fCos, 0.0f, 0.0f,
				              0.0f, 0.0f, 1.0f, 0.0f,
				              0.0f, 0.0f, 0.0f, 1.0f);

				return kZMat * (kYMat * kXMat);
			}

			static Matrix4 fromEulerAnglesZYX(float fYAngle, float fPAngle, float fRAngle)
			{
				float fCos, fSin;

				fCos = cos(fYAngle);
				fSin = sin(fYAngle);
				Matrix4 kZMat(
				    fCos, -fSin, 0.0f, 0.0f,
				    fSin, fCos, 0.0f, 0.0f,
				    0.0f, 0.0f, 1.0f, 0.0f,
				    0.0f, 0.0f, 0.0f, 1.0f);

				fCos = cos(fPAngle);
				fSin = sin(fPAngle);
				Matrix4 kYMat(
				    fCos, 0.0f, fSin, 0.0f,
				    0.0f, 1.0f, 0.0f, 0.0f,
				    -fSin, 0.0f, fCos, 0.0f,
				    0.0f, 0.0f, 0.0f, 1.0f);

				fCos = cos(fRAngle);
				fSin = sin(fRAngle);
				Matrix4 kXMat(
				    1.0f, 0.0f, 0.0f, 0.0f,
				    0.0f, fCos, -fSin, 0.0f,
				    0.0f, fSin, fCos, 0.0f,
				    0.0f, 0.0f, 0.0f, 1.0f);

				return kZMat * (kYMat * kXMat);
			}

			void fromAngleAxis(const Vector3 &rkAxis, float fRadians)
			{
				const float c = cosf(fRadians);
				const float ic = 1.f - c;
				const float s = sinf(fRadians);

				m[0][3] = m[1][3] = m[2][3] = m[3][0] = m[3][1] = m[3][2] = 0.f;
				m[3][3] = 1.f;
				m[0][0] = rkAxis.x * rkAxis.x * ic + c;
				m[0][1] = rkAxis.x * rkAxis.y * ic - rkAxis.z * s;
				m[0][2] = rkAxis.x * rkAxis.z * ic + rkAxis.y * s;
				m[1][0] = rkAxis.y * rkAxis.x * ic + rkAxis.z * s;
				m[1][1] = rkAxis.y * rkAxis.y * ic + c;
				m[1][2] = rkAxis.y * rkAxis.z * ic - rkAxis.x * s;
				m[2][0] = rkAxis.x * rkAxis.z * ic - rkAxis.y * s;
				m[2][1] = rkAxis.y * rkAxis.z * ic + rkAxis.x * s;
				m[2][2] = rkAxis.z * rkAxis.z * ic + c;
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

			inline bool operator == (const Matrix4 &m2) const
			{
				return !(
				           m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
				           m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
				           m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
				           m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3]);
			}

			inline bool operator != (const Matrix4 &m2) const
			{
				return (
				           m[0][0] != m2.m[0][0] || m[0][1] != m2.m[0][1] || m[0][2] != m2.m[0][2] || m[0][3] != m2.m[0][3] ||
				           m[1][0] != m2.m[1][0] || m[1][1] != m2.m[1][1] || m[1][2] != m2.m[1][2] || m[1][3] != m2.m[1][3] ||
				           m[2][0] != m2.m[2][0] || m[2][1] != m2.m[2][1] || m[2][2] != m2.m[2][2] || m[2][3] != m2.m[2][3] ||
				           m[3][0] != m2.m[3][0] || m[3][1] != m2.m[3][1] || m[3][2] != m2.m[3][2] || m[3][3] != m2.m[3][3]);
			}
		};

		io::Writer &operator << (io::Writer &w, Matrix4 const &mat);
		io::Reader &operator >> (io::Reader &r, Matrix4 &mat);
	}
}
