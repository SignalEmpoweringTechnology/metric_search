//=================================================================================================
/*!
//  \file blaze/math/constraints/VecVecMultExpr.h
//  \brief Constraint on the data type
//
//  Copyright (C) 2012-2018 Klaus Iglberger - All Rights Reserved
//
//  This file is part of the Blaze library. You can redistribute it and/or modify it under
//  the terms of the New (Revised) BSD License. Redistribution and use in source and binary
//  forms, with or without modification, are permitted provided that the following conditions
//  are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice, this list
//     of conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//  3. Neither the names of the Blaze development group nor the names of its contributors
//     may be used to endorse or promote products derived from this software without specific
//     prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
//  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
//  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//  DAMAGE.
*/
//=================================================================================================

#ifndef _BLAZE_MATH_CONSTRAINTS_VECVECMULTEXPR_H_
#define _BLAZE_MATH_CONSTRAINTS_VECVECMULTEXPR_H_


//*************************************************************************************************
// Includes
//*************************************************************************************************

#include "../../math/typetraits/IsColumnVector.h"
#include "../../math/typetraits/IsRowVector.h"
#include "../../math/typetraits/IsVecVecMultExpr.h"
#include "../../math/typetraits/Size.h"
#include "../../util/mpl/And.h"
#include "../../util/mpl/Equal.h"
#include "../../util/mpl/Or.h"
#include "../../util/mpl/PtrdiffT.h"


namespace blaze {

//=================================================================================================
//
//  MUST_BE_VECVECMULTEXPR_TYPE CONSTRAINT
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Constraint on the data type.
// \ingroup math_constraints
//
// In case the given data type \a T is not a vector/vector multiplication expression (i.e. a type
// derived from the VecVecMultExpr base class), a compilation error is created.
*/
#define BLAZE_CONSTRAINT_MUST_BE_VECVECMULTEXPR_TYPE(T) \
   static_assert( ::blaze::IsVecVecMultExpr<T>::value, "Non-vector/vector multiplication expression type detected" )
//*************************************************************************************************




//=================================================================================================
//
//  MUST_NOT_BE_VECVECMULTEXPR_TYPE CONSTRAINT
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Constraint on the data type.
// \ingroup math_constraints
//
// In case the given data type \a T is a vector/vector multiplication expression (i.e. a type
// derived from the VecVecMultExpr base class), a compilation error is created.
*/
#define BLAZE_CONSTRAINT_MUST_NOT_BE_VECVECMULTEXPR_TYPE(T) \
   static_assert( !::blaze::IsVecVecMultExpr<T>::value, "Vector/vector multiplication expression type detected" )
//*************************************************************************************************




//=================================================================================================
//
//  MUST_FORM_VALID_VECVECMULTEXPR CONSTRAINT
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Constraint on the data type.
// \ingroup math_constraints
//
// In case the given data types \a T1 and \a T2 do not form a valid vector/vector multiplication,
// a compilation error is created.
*/
#define BLAZE_CONSTRAINT_MUST_FORM_VALID_VECVECMULTEXPR(T1,T2) \
   static_assert( ::blaze::And< ::blaze::Or< ::blaze::And< ::blaze::IsRowVector<T1> \
                                                         , ::blaze::IsRowVector<T2> > \
                                           , ::blaze::And< ::blaze::IsColumnVector<T1> \
                                                         , ::blaze::IsColumnVector<T2> > > \
                              , ::blaze::Or< ::blaze::Equal< ::blaze::Size<T1,0UL>, ::blaze::PtrdiffT<-1L> > \
                                           , ::blaze::Equal< ::blaze::Size<T2,0UL>, ::blaze::PtrdiffT<-1L> > \
                                           , ::blaze::Equal< ::blaze::Size<T1,0UL>, ::blaze::Size<T2,0UL> > > \
                              >::value, "Invalid vector/vector multiplication expression detected" )
//*************************************************************************************************

} // namespace blaze

#endif
