// Nany - https://nany.io
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//

/// \important THIS FILE IS AUTOMATICALLY GENERATED

/// \file    std.core/u16.ny
/// \brief   Implementation of the class u16, Unsigned integer with width of exactly 16 bits
/// \ingroup std.core

public class u16 {
	operator new;

	operator new (cref x: u16) {
		pod = x.pod;
	}

	operator new (cref x: u8) {
		pod = x.pod;
	}

	#[nosuggest] operator new (self pod: __u16);

	#[nosuggest] operator new (self pod: __u8);

	func as<:T:>
		-> new T(!!as(#[__nanyc_synthetic] typeof(std.asBuiltin(new T)), pod));

	operator ++self: ref u16 {
		pod = !!inc(pod);
		return self;
	}

	operator self++: ref u16 {
		ref tmp = new u16(pod);
		pod = !!inc(pod);
		return tmp;
	}

	operator --self: ref u16 {
		pod = !!dec(pod);
		return self;
	}

	operator self--: ref u16 {
		ref tmp = new u16(pod);
		pod = !!dec(pod);
		return tmp;
	}

	operator += (cref x: u16): ref u16 {
		pod = !!add(pod, x.pod);
		return self;
	}

	#[nosuggest] operator += (x: __u16): ref u16 {
		pod = !!add(pod, x);
		return self;
	}

	operator += (cref x: u8): ref u16 {
		pod = !!add(pod, x.pod);
		return self;
	}

	#[nosuggest] operator += (x: __u8): ref u16 {
		pod = !!add(pod, x);
		return self;
	}

	operator -= (cref x: u16): ref u16 {
		pod = !!sub(pod, x.pod);
		return self;
	}

	#[nosuggest] operator -= (x: __u16): ref u16 {
		pod = !!sub(pod, x);
		return self;
	}

	operator -= (cref x: u8): ref u16 {
		pod = !!sub(pod, x.pod);
		return self;
	}

	#[nosuggest] operator -= (x: __u8): ref u16 {
		pod = !!sub(pod, x);
		return self;
	}

	operator *= (cref x: u16): ref u16 {
		pod = !!mul(pod, x.pod);
		return self;
	}

	#[nosuggest] operator *= (x: __u16): ref u16 {
		pod = !!mul(pod, x);
		return self;
	}

	operator *= (cref x: u8): ref u16 {
		pod = !!mul(pod, x.pod);
		return self;
	}

	#[nosuggest] operator *= (x: __u8): ref u16 {
		pod = !!mul(pod, x);
		return self;
	}

	operator /= (cref x: u16): ref u16 {
		pod = !!div(pod, x.pod);
		return self;
	}

	#[nosuggest] operator /= (x: __u16): ref u16 {
		pod = !!div(pod, x);
		return self;
	}

	operator /= (cref x: u8): ref u16 {
		pod = !!div(pod, x.pod);
		return self;
	}

	#[nosuggest] operator /= (x: __u8): ref u16 {
		pod = !!div(pod, x);
		return self;
	}

private:
	var pod = 0__u16;

} // class u16




#[__nanyc_builtinalias: gt] public operator > (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: gt] public operator > (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: gt] public operator > (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: gt] public operator > (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: gt, nosuggest] public operator > (a: __u16, b: __u8): __bool;

#[__nanyc_builtinalias: gte] public operator >= (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: gte] public operator >= (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: gte] public operator >= (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: gte] public operator >= (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: gte, nosuggest] public operator >= (a: __u16, b: __u8): __bool;

#[__nanyc_builtinalias: lt] public operator < (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: lt] public operator < (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: lt] public operator < (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: lt] public operator < (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: lt, nosuggest] public operator < (a: __u16, b: __u8): __bool;

#[__nanyc_builtinalias: lte] public operator <= (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: lte] public operator <= (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: lte] public operator <= (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: lte] public operator <= (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: lte, nosuggest] public operator <= (a: __u16, b: __u8): __bool;





#[__nanyc_builtinalias: eq] public operator == (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: eq] public operator == (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: eq] public operator == (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: eq] public operator == (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: eq, nosuggest] public operator == (a: __u16, b: __u8): __bool;

#[__nanyc_builtinalias: neq] public operator != (a: cref u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: cref u16, b: __u64): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: cref u64): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: __u64): __bool;
#[__nanyc_builtinalias: neq] public operator != (a: cref u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: cref u16, b: __u32): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: cref u32): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: __u32): __bool;
#[__nanyc_builtinalias: neq] public operator != (a: cref u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: cref u16, b: __u16): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: cref u16): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: __u16): __bool;
#[__nanyc_builtinalias: neq] public operator != (a: cref u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: cref u16, b: __u8): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: cref u8): ref bool;
#[__nanyc_builtinalias: neq, nosuggest] public operator != (a: __u16, b: __u8): __bool;





#[__nanyc_builtinalias: add] public operator + (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: add] public operator + (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: add] public operator + (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: add] public operator + (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: add, nosuggest] public operator + (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: sub] public operator - (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: sub] public operator - (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: sub] public operator - (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: sub] public operator - (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: sub, nosuggest] public operator - (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: div] public operator / (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: div] public operator / (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: div] public operator / (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: div] public operator / (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: div, nosuggest] public operator / (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: mul] public operator * (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: mul] public operator * (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: mul] public operator * (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: mul] public operator * (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: mul, nosuggest] public operator * (a: __u16, b: __u8): any;






#[__nanyc_builtinalias: and] public operator and (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: and] public operator and (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: and] public operator and (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: and] public operator and (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: and, nosuggest] public operator and (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: or] public operator or (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: or] public operator or (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: or] public operator or (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: or] public operator or (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: or, nosuggest] public operator or (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: xor] public operator xor (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: xor] public operator xor (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: xor] public operator xor (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: xor] public operator xor (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: xor, nosuggest] public operator xor (a: __u16, b: __u8): any;


#[__nanyc_builtinalias: mod] public operator mod (a: cref u16, b: cref u64): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: cref u16, b: __u64): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: cref u64): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: __u64): any;

#[__nanyc_builtinalias: mod] public operator mod (a: cref u16, b: cref u32): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: cref u16, b: __u32): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: cref u32): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: __u32): any;

#[__nanyc_builtinalias: mod] public operator mod (a: cref u16, b: cref u16): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: cref u16, b: __u16): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: cref u16): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: __u16): any;

#[__nanyc_builtinalias: mod] public operator mod (a: cref u16, b: cref u8): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: cref u16, b: __u8): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: cref u8): any;
#[__nanyc_builtinalias: mod, nosuggest] public operator mod (a: __u16, b: __u8): any;