
#pragma once

#include "utilities.h"

namespace wowpp
{
	template <class T>
	struct Box
	{
		typedef T Type;

		T minimum;
		T maximum;


		Box()
		{
			clear();
		}

		Box(T min_, T max_)
		{
			minimum = min_;
			maximum = max_;
		}

		void clear()
		{
			minimum = T();
			maximum = T();
		}

		Box operator + () const
		{
			return *this;
		}

		Box operator - () const
		{
			Box temp;

			temp.minimum = -minimum;
			temp.maximum = -maximum;

			return temp;
		}
	};

	template <class T>
	Box<T> &operator += (Box<T> &left, const Box<T> &right)
	{
		left.minimum += right.minimum;
		left.maximum += right.maximum;

		return left;
	}

	template <class T>
	Box<T> &operator -= (Box<T> &left, const Box<T> &right)
	{
		left.minimum -= right.minimum;
		left.maximum -= right.maximum;

		return left;
	}

	template <class T>
	Box<T> operator + (const Box<T> &left, const Box<T> &right)
	{
		Box<T> temp(left);
		temp += right;
		return temp;
	}

	template <class T>
	Box<T> operator - (const Box<T> &left, const Box<T> &right)
	{
		Box<T> temp(left);
		temp -= right;
		return temp;
	}


	template <class T>
	bool operator == (const Box<T> &left, const Box<T> &right)
	{
		return (left.minimum == right.minimum) &&
			(left.maximum == right.maximum);
	}

	template <class T>
	bool operator != (const Box<T> &left, const Box<T> &right)
	{
		return !(left == right);
	}
	
	template <class T>
	Box<T> makeBox(const T &minimum, const T &maximum)
	{
		return Box<T>(minimum, maximum);
	}
}
