//=================================================================================================
/*!
//  \file blaze/math/expressions/TDMatDVecMultExpr.h
//  \brief Header file for the transpose dense matrix/dense vector multiplication expression
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

#ifndef _BLAZE_MATH_EXPRESSIONS_TDMATDVECMULTEXPR_H_
#define _BLAZE_MATH_EXPRESSIONS_TDMATDVECMULTEXPR_H_


//*************************************************************************************************
// Includes
//*************************************************************************************************

#include "../../math/blas/gemv.h"
#include "../../math/blas/trmv.h"
#include "../../math/Aliases.h"
#include "../../math/constraints/ColumnMajorMatrix.h"
#include "../../math/constraints/ColumnVector.h"
#include "../../math/constraints/DenseMatrix.h"
#include "../../math/constraints/DenseVector.h"
#include "../../math/constraints/MatMatMultExpr.h"
#include "../../math/constraints/MatVecMultExpr.h"
#include "../../math/constraints/RequiresEvaluation.h"
#include "../../math/Exception.h"
#include "../../math/expressions/Computation.h"
#include "../../math/expressions/DenseVector.h"
#include "../../math/expressions/Forward.h"
#include "../../math/expressions/MatVecMultExpr.h"
#include "../../math/expressions/VecScalarMultExpr.h"
#include "../../math/shims/Reset.h"
#include "../../math/shims/Serial.h"
#include "../../math/SIMD.h"
#include "../../math/traits/MultTrait.h"
#include "../../math/typetraits/HasConstDataAccess.h"
#include "../../math/typetraits/HasMutableDataAccess.h"
#include "../../math/typetraits/HasSIMDAdd.h"
#include "../../math/typetraits/HasSIMDMult.h"
#include "../../math/typetraits/IsAligned.h"
#include "../../math/typetraits/IsBLASCompatible.h"
#include "../../math/typetraits/IsComputation.h"
#include "../../math/typetraits/IsContiguous.h"
#include "../../math/typetraits/IsDiagonal.h"
#include "../../math/typetraits/IsExpression.h"
#include "../../math/typetraits/IsLower.h"
#include "../../math/typetraits/IsPadded.h"
#include "../../math/typetraits/IsSIMDCombinable.h"
#include "../../math/typetraits/IsStrictlyLower.h"
#include "../../math/typetraits/IsStrictlyUpper.h"
#include "../../math/typetraits/IsTriangular.h"
#include "../../math/typetraits/IsUpper.h"
#include "../../math/typetraits/RequiresEvaluation.h"
#include "../../math/typetraits/Size.h"
#include "../../math/views/Check.h"
#include "../../system/BLAS.h"
#include "../../system/Optimizations.h"
#include "../../system/Thresholds.h"
#include "../../util/algorithms/Max.h"
#include "../../util/algorithms/Min.h"
#include "../../util/Assert.h"
#include "../../util/Complex.h"
#include "../../util/constraints/SameType.h"
#include "../../util/DisableIf.h"
#include "../../util/EnableIf.h"
#include "../../util/FunctionTrace.h"
#include "../../util/mpl/And.h"
#include "../../util/mpl/If.h"
#include "../../util/Types.h"
#include "../../util/typetraits/IsBuiltin.h"
#include "../../util/typetraits/IsComplex.h"
#include "../../util/typetraits/IsComplexDouble.h"
#include "../../util/typetraits/IsComplexFloat.h"
#include "../../util/typetraits/IsDouble.h"
#include "../../util/typetraits/IsFloat.h"
#include "../../util/typetraits/IsSame.h"


namespace blaze {

//=================================================================================================
//
//  CLASS TDMATDVECMULTEXPR
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Expression object for transpose dense matrix-dense vector multiplications.
// \ingroup dense_vector_expression
//
// The TDMatDVecMultExpr class represents the compile time expression for multiplications
// between column-major dense matrices and dense vectors.
*/
template< typename MT    // Type of the left-hand side dense matrix
        , typename VT >  // Type of the right-hand side dense vector
class TDMatDVecMultExpr
   : public MatVecMultExpr< DenseVector< TDMatDVecMultExpr<MT,VT>, false > >
   , private Computation
{
 private:
   //**Type definitions****************************************************************************
   using MRT = ResultType_<MT>;     //!< Result type of the left-hand side dense matrix expression.
   using VRT = ResultType_<VT>;     //!< Result type of the right-hand side dense vector expression.
   using MET = ElementType_<MRT>;   //!< Element type of the left-hand side dense matrix expression.
   using VET = ElementType_<VRT>;   //!< Element type of the right-hand side dense vector expression.
   using MCT = CompositeType_<MT>;  //!< Composite type of the left-hand side dense matrix expression.
   using VCT = CompositeType_<VT>;  //!< Composite type of the right-hand side dense vector expression.
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the left-hand side dense matrix expression.
   enum : bool { evaluateMatrix = ( IsComputation<MT>::value && IsSame<MET,VET>::value &&
                                    IsBLASCompatible<MET>::value ) || RequiresEvaluation<MT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the right-hand side dense vector expression.
   enum : bool { evaluateVector = IsComputation<VT>::value || RequiresEvaluation<VT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! The UseSMPAssign struct is a helper struct for the selection of the parallel evaluation
       strategy. In case either the matrix or the vector operand requires an intermediate
       evaluation, the nested \a value will be set to 1, otherwise it will be 0. */
   template< typename T1 >
   struct UseSMPAssign {
      enum : bool { value = ( evaluateMatrix || evaluateVector ) };
   };
   /*! \endcond */
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case the matrix type and the two involved vector types are suited for a BLAS kernel,
       the nested \a value will be set to 1, otherwise it will be 0. */
   template< typename T1, typename T2, typename T3 >
   struct UseBlasKernel {
      enum : bool { value = BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION &&
                            IsContiguous<T1>::value && HasMutableDataAccess<T1>::value &&
                            IsContiguous<T2>::value && HasConstDataAccess<T2>::value &&
                            IsContiguous<T3>::value && HasConstDataAccess<T3>::value &&
                            !IsDiagonal<T2>::value &&
                            T1::simdEnabled && T2::simdEnabled && T3::simdEnabled &&
                            IsBLASCompatible< ElementType_<T1> >::value &&
                            IsBLASCompatible< ElementType_<T2> >::value &&
                            IsBLASCompatible< ElementType_<T3> >::value &&
                            IsSame< ElementType_<T1>, ElementType_<T2> >::value &&
                            IsSame< ElementType_<T1>, ElementType_<T3> >::value };
   };
   /*! \endcond */
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case the matrix type and the two involved vector types are suited for a vectorized
       computation of the matrix/vector multiplication, the nested \a value will be set to 1,
       otherwise it will be 0. */
   template< typename T1, typename T2, typename T3 >
   struct UseVectorizedDefaultKernel {
      enum : bool { value = useOptimizedKernels &&
                            !IsDiagonal<T2>::value &&
                            T1::simdEnabled && T2::simdEnabled && T3::simdEnabled &&
                            IsSIMDCombinable< ElementType_<T1>
                                            , ElementType_<T2>
                                            , ElementType_<T3> >::value &&
                            HasSIMDAdd< ElementType_<T2>, ElementType_<T3> >::value &&
                            HasSIMDMult< ElementType_<T2>, ElementType_<T3> >::value };
   };
   /*! \endcond */
   //**********************************************************************************************

 public:
   //**Type definitions****************************************************************************
   using This          = TDMatDVecMultExpr<MT,VT>;    //!< Type of this TDMatDVecMultExpr instance.
   using ResultType    = MultTrait_<MRT,VRT>;         //!< Result type for expression template evaluations.
   using TransposeType = TransposeType_<ResultType>;  //!< Transpose type for expression template evaluations.
   using ElementType   = ElementType_<ResultType>;    //!< Resulting element type.
   using SIMDType      = SIMDTrait_<ElementType>;     //!< Resulting SIMD element type.
   using ReturnType    = const ElementType;           //!< Return type for expression template evaluations.
   using CompositeType = const ResultType;            //!< Data type for composite expression templates.

   //! Composite type of the left-hand side dense matrix expression.
   using LeftOperand = If_< IsExpression<MT>, const MT, const MT& >;

   //! Composite type of the right-hand side dense vector expression.
   using RightOperand = If_< IsExpression<VT>, const VT, const VT& >;

   //! Type for the assignment of the left-hand side dense matrix operand.
   using LT = IfTrue_< evaluateMatrix, const MRT, MCT >;

   //! Type for the assignment of the right-hand side dense vector operand.
   using RT = IfTrue_< evaluateVector, const VRT, VCT >;
   //**********************************************************************************************

   //**Compilation flags***************************************************************************
   //! Compilation switch for the expression template evaluation strategy.
   enum : bool { simdEnabled = !IsDiagonal<MT>::value &&
                               MT::simdEnabled && VT::simdEnabled &&
                               HasSIMDAdd<MET,VET>::value &&
                               HasSIMDMult<MET,VET>::value };

   //! Compilation switch for the expression template assignment strategy.
   enum : bool { smpAssignable = !evaluateMatrix && MT::smpAssignable &&
                                 !evaluateVector && VT::smpAssignable };
   //**********************************************************************************************

   //**SIMD properties*****************************************************************************
   //! The number of elements packed within a single SIMD element.
   enum : size_t { SIMDSIZE = SIMDTrait<ElementType>::size };
   //**********************************************************************************************

   //**Constructor*********************************************************************************
   /*!\brief Constructor for the TDMatDVecMultExpr class.
   //
   // \param mat The left-hand side matrix operand of the multiplication expression.
   // \param vec The right-hand side vector operand of the multiplication expression.
   */
   explicit inline TDMatDVecMultExpr( const MT& mat, const VT& vec ) noexcept
      : mat_( mat )  // Left-hand side dense matrix of the multiplication expression
      , vec_( vec )  // Right-hand side dense vector of the multiplication expression
   {
      BLAZE_INTERNAL_ASSERT( mat_.columns() == vec_.size(), "Invalid matrix and vector sizes" );
   }
   //**********************************************************************************************

   //**Subscript operator**************************************************************************
   /*!\brief Subscript operator for the direct access to the vector elements.
   //
   // \param index Access index. The index has to be in the range \f$[0..N-1]\f$.
   // \return The resulting value.
   */
   inline ReturnType operator[]( size_t index ) const {
      BLAZE_INTERNAL_ASSERT( index < mat_.rows(), "Invalid vector access index" );

      if( IsDiagonal<MT>::value )
      {
         return mat_(index,index) * vec_[index];
      }
      else if( IsLower<MT>::value && ( index + 8UL < mat_.rows() ) )
      {
         const size_t n( IsStrictlyLower<MT>::value ? index : index+1UL );
         return subvector( row( mat_, index, unchecked ), 0UL, n, unchecked ) *
                subvector( vec_, 0UL, n, unchecked );
      }
      else if( IsUpper<MT>::value && ( index > 8UL ) )
      {
         const size_t begin( IsStrictlyUpper<MT>::value ? index+1UL : index );
         const size_t n    ( mat_.columns() - begin );
         return subvector( row( mat_, index, unchecked ), begin, n, unchecked ) *
                subvector( vec_, begin, n, unchecked );
      }
      else
      {
         return row( mat_, index, unchecked ) * vec_;
      }
   }
   //**********************************************************************************************

   //**At function*********************************************************************************
   /*!\brief Checked access to the vector elements.
   //
   // \param index Access index. The index has to be in the range \f$[0..N-1]\f$.
   // \return The resulting value.
   // \exception std::out_of_range Invalid vector access index.
   */
   inline ReturnType at( size_t index ) const {
      if( index >= mat_.rows() ) {
         BLAZE_THROW_OUT_OF_RANGE( "Invalid vector access index" );
      }
      return (*this)[index];
   }
   //**********************************************************************************************

   //**Size function*******************************************************************************
   /*!\brief Returns the current size/dimension of the vector.
   //
   // \return The size of the vector.
   */
   inline size_t size() const noexcept {
      return mat_.rows();
   }
   //**********************************************************************************************

   //**Left operand access*************************************************************************
   /*!\brief Returns the left-hand side transpose dense matrix operand.
   //
   // \return The left-hand side transpose dense matrix operand.
   */
   inline LeftOperand leftOperand() const noexcept {
      return mat_;
   }
   //**********************************************************************************************

   //**Right operand access************************************************************************
   /*!\brief Returns the right-hand side dense vector operand.
   //
   // \return The right-hand side dense vector operand.
   */
   inline RightOperand rightOperand() const noexcept {
      return vec_;
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can alias with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case the expression can alias, \a false otherwise.
   */
   template< typename T >
   inline bool canAlias( const T* alias ) const noexcept {
      return ( mat_.isAliased( alias ) || vec_.isAliased( alias ) );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression is aliased with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case an alias effect is detected, \a false otherwise.
   */
   template< typename T >
   inline bool isAliased( const T* alias ) const noexcept {
      return ( mat_.isAliased( alias ) || vec_.isAliased( alias ) );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the operands of the expression are properly aligned in memory.
   //
   // \return \a true in case the operands are aligned, \a false if not.
   */
   inline bool isAligned() const noexcept {
      return mat_.isAligned() && vec_.isAligned();
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can be used in SMP assignments.
   //
   // \return \a true in case the expression can be used in SMP assignments, \a false if not.
   */
   inline bool canSMPAssign() const noexcept {
      return ( !BLAZE_BLAS_MODE ||
               !BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION ||
               !BLAZE_BLAS_IS_PARALLEL ||
               ( IsComputation<MT>::value && !evaluateMatrix ) ||
               ( mat_.rows() * mat_.columns() < TDMATDVECMULT_THRESHOLD ) ) &&
             ( size() > SMP_TDMATDVECMULT_THRESHOLD );
   }
   //**********************************************************************************************

 private:
   //**Member variables****************************************************************************
   LeftOperand  mat_;  //!< Left-hand side dense matrix of the multiplication expression.
   RightOperand vec_;  //!< Right-hand side dense vector of the multiplication expression.
   //**********************************************************************************************

   //**Assignment to dense vectors*****************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Assignment of a transpose dense matrix-dense vector multiplication to a dense vector
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a transpose dense matrix-
   // dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void assign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL ) {
         return;
      }
      else if( rhs.mat_.columns() == 0UL ) {
         reset( ~lhs );
         return;
      }

      LT A( serial( rhs.mat_ ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( rhs.vec_ ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      TDMatDVecMultExpr::selectAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Assignment to dense vectors (kernel selection)**********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Selection of the kernel for an assignment of a transpose dense matrix-dense vector
   //        multiplication to a dense vector (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallAssignKernel( y, A, x );
      else
         selectBlasAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default assignment to dense vectors*********************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the default assignment kernel for the transpose dense matrix-dense
   // vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectDefaultAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      if( IsStrictlyLower<MT1>::value ) {
         reset( y[0] );
      }

      if( !IsUpper<MT1>::value )
      {
         for( size_t i=( IsStrictlyLower<MT1>::value ? 1UL : 0UL ); i<M; ++i ) {
            y[i] = A(i,0UL) * x[0UL];
         }
      }

      for( size_t j=( IsUpper<MT1>::value && !IsStrictlyUpper<MT1>::value ? 0UL : 1UL ); j<N; ++j )
      {
         if( IsDiagonal<MT1>::value )
         {
            y[j] = A(j,j) * x[j];
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? j+1UL : j )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? j-1UL : j )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            const size_t inum( iend - ibegin );
            const size_t ipos( ibegin + ( inum & size_t(-2) ) );

            for( size_t i=ibegin; i<ipos; i+=2UL ) {
               y[i    ] += A(i    ,j) * x[j];
               y[i+1UL] += A(i+1UL,j) * x[j];
            }
            if( ipos < iend ) {
               y[ipos] += A(ipos,j) * x[j];
            }
            if( IsUpper<MT1>::value ) {
               y[iend] = A(iend,j) * x[j];
            }
         }
      }

      if( IsStrictlyUpper<MT1>::value ) {
         reset( y[M-1UL] );
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default assignment to dense vectors (small matrices)****************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default assignment of a small transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the assignment of a transpose dense
   // matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default assignment to dense vectors (small matrices)*****************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default assignment of a small transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default assignment kernel for the transpose dense
   // matrix-dense vector multiplication. This kernel is optimized for small matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
         y.store( i+SIMDSIZE*4UL, xmm5 );
         y.store( i+SIMDSIZE*5UL, xmm6 );
         y.store( i+SIMDSIZE*6UL, xmm7 );
         y.store( i+SIMDSIZE*7UL, xmm8 );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i         ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , xmm1 );
         y.store( i+SIMDSIZE, xmm2 );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1;

         for( size_t j=jbegin; j<jend; ++j ) {
            xmm1 += A.load(i,j) * set( x[j] );
         }

         y.store( i, xmm1 );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] = value;
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default assignment to dense vectors (large matrices)****************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default assignment of a large transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the assignment of a transpose dense
   // matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default assignment to dense vectors (large matrices)*****************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default assignment of a large transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default assignment kernel for the transpose dense
   // matrix-dense vector multiplication. This kernel is optimized for large matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      reset( y );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4 );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) + xmm5 );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) + xmm6 );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) + xmm7 );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) + xmm8 );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4 );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) + xmm1 );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) + xmm2 );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) + xmm1 );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] += value;
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based assignment to dense vectors (default)********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the assignment of a large transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectLargeAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based assignment to dense vectors******************************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*! \cond BLAZE_INTERNAL */
   /*!\brief BLAS-based assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function performs the transpose dense matrix-dense vector multiplication based on the
   // according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         assign( y, x );
         trmv( y, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
      }
      else {
         gemv( y, A, x, ET(1), ET(0) );
      }
   }
   /*! \endcond */
#endif
   //**********************************************************************************************

   //**Assignment to sparse vectors****************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Assignment of a transpose dense matrix-dense vector multiplication to a sparse vector
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a transpose dense matrix-
   // dense vector multiplication expression to a sparse vector.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline void assign( SparseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      assign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Addition assignment to dense vectors********************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Addition assignment of a transpose dense matrix-dense vector multiplication to a dense
   //        vector (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized addition assignment of a transpose dense
   // matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void addAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL || rhs.mat_.columns() == 0UL ) {
         return;
      }

      LT A( serial( rhs.mat_ ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( rhs.vec_ ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      TDMatDVecMultExpr::selectAddAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Addition assignment to dense vectors (kernel selection)*************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Selection of the kernel for an addition assignment of a transpose dense matrix-dense
   //        vector multiplication to a dense vector (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallAddAssignKernel( y, A, x );
      else
         selectBlasAddAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default addition assignment to dense vectors************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default addition assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the default addition assignment kernel for the transpose dense
   // matrix-dense vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectDefaultAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      for( size_t j=0UL; j<N; ++j )
      {
         if( IsDiagonal<MT1>::value )
         {
            y[j] += A(j,j) * x[j];
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? j+1UL : j )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? j : j+1UL )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            const size_t inum( iend - ibegin );
            const size_t ipos( ibegin + ( inum & size_t(-2) ) );

            for( size_t i=ibegin; i<ipos; i+=2UL ) {
               y[i    ] += A(i    ,j) * x[j];
               y[i+1UL] += A(i+1UL,j) * x[j];
            }
            if( ipos < iend ) {
               y[ipos] += A(ipos,j) * x[j];
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default addition assignment to dense vectors (small matrices)*******************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default addition assignment of a small transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultAddAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default addition assignment to dense vectors (small matrices)********************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default addition assignment of a small transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default addition assignment kernel for the transpose
   // dense matrix-dense vector multiplication. This kernel is optimized for small matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );
         SIMDType xmm4( y.load(i+SIMDSIZE*3UL) );
         SIMDType xmm5( y.load(i+SIMDSIZE*4UL) );
         SIMDType xmm6( y.load(i+SIMDSIZE*5UL) );
         SIMDType xmm7( y.load(i+SIMDSIZE*6UL) );
         SIMDType xmm8( y.load(i+SIMDSIZE*7UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
         y.store( i+SIMDSIZE*4UL, xmm5 );
         y.store( i+SIMDSIZE*5UL, xmm6 );
         y.store( i+SIMDSIZE*6UL, xmm7 );
         y.store( i+SIMDSIZE*7UL, xmm8 );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );
         SIMDType xmm4( y.load(i+SIMDSIZE*3UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i         ) );
         SIMDType xmm2( y.load(i+SIMDSIZE) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i         ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , xmm1 );
         y.store( i+SIMDSIZE, xmm2 );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i) );

         for( size_t j=jbegin; j<jend; ++j ) {
            xmm1 += A.load(i,j) * set( x[j] );
         }

         y.store( i, xmm1 );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] += value;
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default addition assignment to dense vectors (large matrices)*******************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default addition assignment of a large transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultAddAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default addition assignment to dense vectors (large matrices)********************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default addition assignment of a large transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default addition assignment kernel for the transpose
   // dense matrix-dense vector multiplication. This kernel is optimized for large matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4 );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) + xmm5 );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) + xmm6 );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) + xmm7 );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) + xmm8 );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4 );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3 );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) + xmm1 );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) + xmm2 );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) + xmm1 );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] += value;
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based addition assignment to dense vectors (default)***********************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default addition assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a large
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectLargeAddAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based addition assignment to dense vectors*********************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*! \cond BLAZE_INTERNAL */
   /*!\brief BLAS-based addition assignment of a transpose matrix-vector multiplication
   //        (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function performs the transpose dense matrix-dense vector multiplication based on the
   // according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         ResultType_<VT1> tmp( serial( x ) );
         trmv( tmp, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
         addAssign( y, tmp );
      }
      else {
         gemv( y, A, x, ET(1), ET(1) );
      }
   }
   /*! \endcond */
#endif
   //**********************************************************************************************

   //**Addition assignment to sparse vectors*******************************************************
   // No special implementation for the addition assignment to sparse vectors.
   //**********************************************************************************************

   //**Subtraction assignment to dense vectors*****************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Subtraction assignment of a transpose dense matrix-dense vector multiplication to a
   //        dense vector (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized subtraction assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void subAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL || rhs.mat_.columns() == 0UL ) {
         return;
      }

      LT A( serial( rhs.mat_ ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( rhs.vec_ ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      TDMatDVecMultExpr::selectSubAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Subtraction assignment to dense vectors (kernel selection)**********************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Selection of the kernel for a subtraction assignment of a transpose dense matrix-
   //        dense vector multiplication to a dense vector (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallSubAssignKernel( y, A, x );
      else
         selectBlasSubAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors*********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default subtraction assignment of a transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the default subtraction assignment kernel for the transpose dense
   // matrix-dense vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline void selectDefaultSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      for( size_t j=0UL; j<N; ++j )
      {
         if( IsDiagonal<MT1>::value )
         {
            y[j] -= A(j,j) * x[j];
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? j+1UL : j )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? j : j+1UL )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            const size_t inum( iend - ibegin );
            const size_t ipos( ibegin + ( inum & size_t(-2) ) );

            for( size_t i=ibegin; i<ipos; i+=2UL ) {
               y[i    ] -= A(i    ,j) * x[j];
               y[i+1UL] -= A(i+1UL,j) * x[j];
            }
            if( ipos < iend ) {
               y[ipos] -= A(ipos,j) * x[j];
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors (small matrices)****************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default subtraction assignment of a small transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultSubAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default subtraction assignment to dense vectors (small matrices)*****************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default subtraction assignment of a small transpose dense matrix-dense
   //        vector multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default subtraction assignment kernel for the
   // transpose dense matrix-dense vector multiplication. This kernel is optimized for small
   // matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectSmallSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );
         SIMDType xmm4( y.load(i+SIMDSIZE*3UL) );
         SIMDType xmm5( y.load(i+SIMDSIZE*4UL) );
         SIMDType xmm6( y.load(i+SIMDSIZE*5UL) );
         SIMDType xmm7( y.load(i+SIMDSIZE*6UL) );
         SIMDType xmm8( y.load(i+SIMDSIZE*7UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 -= A.load(i             ,j) * x1;
            xmm2 -= A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 -= A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 -= A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 -= A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 -= A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 -= A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 -= A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
         y.store( i+SIMDSIZE*4UL, xmm5 );
         y.store( i+SIMDSIZE*5UL, xmm6 );
         y.store( i+SIMDSIZE*6UL, xmm7 );
         y.store( i+SIMDSIZE*7UL, xmm8 );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );
         SIMDType xmm4( y.load(i+SIMDSIZE*3UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 -= A.load(i             ,j) * x1;
            xmm2 -= A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 -= A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 -= A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
         y.store( i+SIMDSIZE*3UL, xmm4 );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i             ) );
         SIMDType xmm2( y.load(i+SIMDSIZE    ) );
         SIMDType xmm3( y.load(i+SIMDSIZE*2UL) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 -= A.load(i             ,j) * x1;
            xmm2 -= A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 -= A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , xmm1 );
         y.store( i+SIMDSIZE    , xmm2 );
         y.store( i+SIMDSIZE*2UL, xmm3 );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i         ) );
         SIMDType xmm2( y.load(i+SIMDSIZE) );

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 -= A.load(i         ,j) * x1;
            xmm2 -= A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , xmm1 );
         y.store( i+SIMDSIZE, xmm2 );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1( y.load(i) );

         for( size_t j=jbegin; j<jend; ++j ) {
            xmm1 -= A.load(i,j) * set( x[j] );
         }

         y.store( i, xmm1 );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] -= value;
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors (large matrices)****************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default subtraction assignment of a large transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectDefaultSubAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized default subtraction assignment to dense vectors (large matrices)*****************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized default subtraction assignment of a large transpose dense matrix-dense
   //        vector multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function implements the vectorized default subtraction assignment kernel for the
   // transpose dense matrix-dense vector multiplication. This kernel is optimized for large
   // matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2> >
      selectLargeSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4 );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) - xmm5 );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) - xmm6 );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) - xmm7 );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) - xmm8 );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3 );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4 );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1 );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2 );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3 );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) - xmm1 );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) - xmm2 );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) - xmm1 );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] -= value;
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based subtraction assignment to dense vectors (default)********************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default subtraction assignment of a transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function relays to the default implementation of the subtraction assignment of a large
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      selectLargeSubAssignKernel( y, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**BLAS-based subtraction assignment to dense vectors******************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*! \cond BLAZE_INTERNAL */
   /*!\brief BLAS-based subtraction assignment of a transpose matrix-vector multiplication
   //        (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \return void
   //
   // This function performs the transpose dense matrix-dense vector multiplication based on the
   // according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2> >
      selectBlasSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         ResultType_<VT1> tmp( serial( x ) );
         trmv( tmp, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
         subAssign( y, tmp );
      }
      else {
         gemv( y, A, x, ET(-1), ET(1) );
      }
   }
   /*! \endcond */
#endif
   //**********************************************************************************************

   //**Subtraction assignment to sparse vectors****************************************************
   // No special implementation for the subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**Multiplication assignment to dense vectors**************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Multiplication assignment of a transpose dense matrix-dense vector multiplication to
   //        a dense vector (\f$ \vec{y}*=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized multiplication assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void multAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      multAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Multiplication assignment to sparse vectors*************************************************
   // No special implementation for the multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**Division assignment to dense vectors********************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Division assignment of a transpose dense matrix-dense vector multiplication to a
   //        dense vector (\f$ \vec{y}/=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression divisor.
   // \return void
   //
   // This function implements the performance optimized division assignment of a transpose dense
   // matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void divAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      divAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Division assignment to sparse vectors*******************************************************
   // No special implementation for the division assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP assignment to dense vectors*************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP assignment of a transpose dense matrix-dense vector multiplication to a dense
   //        vector (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a transpose dense
   // matrix-dense vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler
   // in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL ) {
         return;
      }
      else if( rhs.mat_.columns() == 0UL ) {
         reset( ~lhs );
         return;
      }

      LT A( rhs.mat_ );  // Evaluation of the left-hand side dense matrix operand
      RT x( rhs.vec_ );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      smpAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP assignment to sparse vectors************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP assignment of a transpose dense matrix-dense vector multiplication to a sparse
   //        vector (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a transpose dense
   // matrix-dense vector multiplication expression to a sparse vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler
   // in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAssign( SparseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP addition assignment to dense vectors****************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP addition assignment of a transpose dense matrix-dense vector multiplication to a
   //        dense vector (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized SMP addition assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAddAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL || rhs.mat_.columns() == 0UL ) {
         return;
      }

      LT A( rhs.mat_ );  // Evaluation of the left-hand side dense matrix operand
      RT x( rhs.vec_ );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      smpAddAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP addition assignment to sparse vectors***************************************************
   // No special implementation for the SMP addition assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP subtraction assignment to dense vectors*************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP subtraction assignment of a transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized SMP subtraction assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpSubAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      if( rhs.mat_.rows() == 0UL || rhs.mat_.columns() == 0UL ) {
         return;
      }

      LT A( rhs.mat_ );  // Evaluation of the left-hand side dense matrix operand
      RT x( rhs.vec_ );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      smpSubAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP subtraction assignment to sparse vectors************************************************
   // No special implementation for the SMP subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP multiplication assignment to dense vectors**********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP multiplication assignment of a transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}*=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized SMP multiplication assignment of a
   // transpose dense matrix-dense vector multiplication expression to a dense vector. Due to
   // the explicit application of the SFINAE principle, this function can only be selected by
   // the compiler in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpMultAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpMultAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP multiplication assignment to sparse vectors*********************************************
   // No special implementation for the SMP multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP division assignment to dense vectors****************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP division assignment of a transpose dense matrix-dense vector multiplication to
   //        a dense vector (\f$ \vec{y}/=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression divisor.
   // \return void
   //
   // This function implements the performance optimized SMP division assignment of a transpose
   // dense matrix-dense vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpDivAssign( DenseVector<VT1,false>& lhs, const TDMatDVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpDivAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP division assignment to sparse vectors***************************************************
   // No special implementation for the SMP division assignment to sparse vectors.
   //**********************************************************************************************

   //**Compile time checks*************************************************************************
   /*! \cond BLAZE_INTERNAL */
   BLAZE_CONSTRAINT_MUST_BE_DENSE_MATRIX_TYPE( MT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_MAJOR_MATRIX_TYPE( MT );
   BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( VT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( VT );
   BLAZE_CONSTRAINT_MUST_FORM_VALID_MATVECMULTEXPR( MT, VT );
   /*! \endcond */
   //**********************************************************************************************
};
//*************************************************************************************************




//=================================================================================================
//
//  DVECSCALARMULTEXPR SPECIALIZATION
//
//=================================================================================================

//*************************************************************************************************
/*! \cond BLAZE_INTERNAL */
/*!\brief Expression object for scaled transpose dense matrix-dense vector multiplications.
// \ingroup dense_vector_expression
//
// This specialization of the DVecScalarMultExpr class represents the compile time expression
// for scaled multiplications between a column-major dense matrix and a non-transpose dense
// vector.
*/
template< typename MT    // Type of the left-hand side dense matrix
        , typename VT    // Type of the right-hand side dense vector
        , typename ST >  // Type of the side scalar value
class DVecScalarMultExpr< TDMatDVecMultExpr<MT,VT>, ST, false >
   : public VecScalarMultExpr< DenseVector< DVecScalarMultExpr< TDMatDVecMultExpr<MT,VT>, ST, false >, false > >
   , private Computation
{
 private:
   //**Type definitions****************************************************************************
   using MVM = TDMatDVecMultExpr<MT,VT>;  //!< Type of the transpose dense matrix-dense vector multiplication expression.
   using RES = ResultType_<MVM>;          //!< Result type of the dense matrix-dense vector multiplication expression.
   using MRT = ResultType_<MT>;           //!< Result type of the left-hand side dense matrix expression.
   using VRT = ResultType_<VT>;           //!< Result type of the right-hand side dense vector expression.
   using MET = ElementType_<MRT>;         //!< Element type of the left-hand side dense matrix expression.
   using VET = ElementType_<VRT>;         //!< Element type of the right-hand side dense vector expression.
   using MCT = CompositeType_<MT>;        //!< Composite type of the left-hand side dense matrix expression.
   using VCT = CompositeType_<VT>;        //!< Composite type of the right-hand side dense vector expression.
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the right-hand side dense matrix expression.
   enum : bool { evaluateMatrix = ( IsComputation<MT>::value && IsSame<MET,VET>::value &&
                                    IsBLASCompatible<MET>::value ) || RequiresEvaluation<MT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the right-hand side dense vector expression.
   enum : bool { evaluateVector = IsComputation<VT>::value || RequiresEvaluation<VT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! The UseSMPAssign struct is a helper struct for the selection of the parallel evaluation
       strategy. In case either the matrix or the vector operand requires an intermediate
       evaluation, the nested \a value will be set to 1, otherwise it will be 0. */
   template< typename T1 >
   struct UseSMPAssign {
      enum : bool { value = ( evaluateMatrix || evaluateVector ) };
   };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case the matrix type, the two involved vector types, and the scalar type are suited
       for a BLAS kernel, the nested \a value will be set to 1, otherwise it will be 0. */
   template< typename T1, typename T2, typename T3, typename T4 >
   struct UseBlasKernel {
      enum : bool { value = BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION &&
                            IsContiguous<T1>::value && HasMutableDataAccess<T1>::value &&
                            IsContiguous<T2>::value && HasConstDataAccess<T2>::value &&
                            IsContiguous<T3>::value && HasConstDataAccess<T3>::value &&
                            !IsDiagonal<T2>::value &&
                            T1::simdEnabled && T2::simdEnabled && T3::simdEnabled &&
                            IsBLASCompatible< ElementType_<T1> >::value &&
                            IsBLASCompatible< ElementType_<T2> >::value &&
                            IsBLASCompatible< ElementType_<T3> >::value &&
                            IsSame< ElementType_<T1>, ElementType_<T2> >::value &&
                            IsSame< ElementType_<T1>, ElementType_<T3> >::value &&
                            !( IsBuiltin< ElementType_<T1> >::value && IsComplex<T4>::value ) };
   };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case the two involved vector types, the matrix type, and the scalar type are suited
       for a vectorized computation of the scaled vector/matrix multiplication, the nested
       \a value will be set to 1, otherwise it will be 0. */
   template< typename T1, typename T2, typename T3, typename T4 >
   struct UseVectorizedDefaultKernel {
      enum : bool { value = useOptimizedKernels &&
                            !IsDiagonal<T2>::value &&
                            T1::simdEnabled && T2::simdEnabled && T3::simdEnabled &&
                            IsSIMDCombinable< ElementType_<T1>
                                            , ElementType_<T2>
                                            , ElementType_<T3>
                                            , T4 >::value &&
                            HasSIMDAdd< ElementType_<T2>, ElementType_<T3> >::value &&
                            HasSIMDMult< ElementType_<T2>, ElementType_<T3> >::value };
   };
   //**********************************************************************************************

 public:
   //**Type definitions****************************************************************************
   using This          = DVecScalarMultExpr<MVM,ST,false>;  //!< Type of this DVecScalarMultExpr instance.
   using ResultType    = MultTrait_<RES,ST>;                //!< Result type for expression template evaluations.
   using TransposeType = TransposeType_<ResultType>;        //!< Transpose type for expression template evaluations.
   using ElementType   = ElementType_<ResultType>;          //!< Resulting element type.
   using SIMDType      = SIMDTrait_<ElementType>;           //!< Resulting SIMD element type.
   using ReturnType    = const ElementType;                 //!< Return type for expression template evaluations.
   using CompositeType = const ResultType;                  //!< Data type for composite expression templates.

   //! Composite type of the left-hand side dense vector expression.
   using LeftOperand = const TDMatDVecMultExpr<MT,VT>;

   //! Composite type of the right-hand side scalar value.
   using RightOperand = ST;

   //! Type for the assignment of the dense matrix operand of the left-hand side expression.
   using LT = IfTrue_< evaluateMatrix, const MRT, MCT >;

   //! Type for the assignment of the dense vector operand of the left-hand side expression.
   using RT = IfTrue_< evaluateVector, const VRT, VCT >;
   //**********************************************************************************************

   //**Compilation flags***************************************************************************
   //! Compilation switch for the expression template evaluation strategy.
   enum : bool { simdEnabled = !IsDiagonal<MT>::value &&
                               MT::simdEnabled && VT::simdEnabled &&
                               IsSIMDCombinable<MET,VET,ST>::value &&
                               HasSIMDAdd<MET,VET>::value &&
                               HasSIMDMult<MET,VET>::value };

   //! Compilation switch for the expression template assignment strategy.
   enum : bool { smpAssignable = !evaluateMatrix && MT::smpAssignable &&
                                 !evaluateVector && VT::smpAssignable };
   //**********************************************************************************************

   //**SIMD properties*****************************************************************************
   //! The number of elements packed within a single SIMD element.
   enum : size_t { SIMDSIZE = SIMDTrait<ElementType>::size };
   //**********************************************************************************************

   //**Constructor*********************************************************************************
   /*!\brief Constructor for the DVecScalarMultExpr class.
   //
   // \param vector The left-hand side dense vector of the multiplication expression.
   // \param scalar The right-hand side scalar of the multiplication expression.
   */
   explicit inline DVecScalarMultExpr( const MVM& vector, ST scalar )
      : vector_( vector )  // Left-hand side dense vector of the multiplication expression
      , scalar_( scalar )  // Right-hand side scalar of the multiplication expression
   {}
   //**********************************************************************************************

   //**Subscript operator**************************************************************************
   /*!\brief Subscript operator for the direct access to the vector elements.
   //
   // \param index Access index. The index has to be in the range \f$[0..N-1]\f$.
   // \return The resulting value.
   */
   inline ReturnType operator[]( size_t index ) const {
      BLAZE_INTERNAL_ASSERT( index < vector_.size(), "Invalid vector access index" );
      return vector_[index] * scalar_;
   }
   //**********************************************************************************************

   //**At function*********************************************************************************
   /*!\brief Checked access to the vector elements.
   //
   // \param index Access index. The index has to be in the range \f$[0..N-1]\f$.
   // \return The resulting value.
   // \exception std::out_of_range Invalid vector access index.
   */
   inline ReturnType at( size_t index ) const {
      if( index >= vector_.size() ) {
         BLAZE_THROW_OUT_OF_RANGE( "Invalid vector access index" );
      }
      return (*this)[index];
   }
   //**********************************************************************************************

   //**Size function*******************************************************************************
   /*!\brief Returns the current size/dimension of the vector.
   //
   // \return The size of the vector.
   */
   inline size_t size() const {
      return vector_.size();
   }
   //**********************************************************************************************

   //**Left operand access*************************************************************************
   /*!\brief Returns the left-hand side dense vector operand.
   //
   // \return The left-hand side dense vector operand.
   */
   inline LeftOperand leftOperand() const {
      return vector_;
   }
   //**********************************************************************************************

   //**Right operand access************************************************************************
   /*!\brief Returns the right-hand side scalar operand.
   //
   // \return The right-hand side scalar operand.
   */
   inline RightOperand rightOperand() const {
      return scalar_;
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can alias with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case the expression can alias, \a false otherwise.
   */
   template< typename T >
   inline bool canAlias( const T* alias ) const {
      return vector_.canAlias( alias );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression is aliased with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case an alias effect is detected, \a false otherwise.
   */
   template< typename T >
   inline bool isAliased( const T* alias ) const {
      return vector_.isAliased( alias );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the operands of the expression are properly aligned in memory.
   //
   // \return \a true in case the operands are aligned, \a false if not.
   */
   inline bool isAligned() const {
      return vector_.isAligned();
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can be used in SMP assignments.
   //
   // \return \a true in case the expression can be used in SMP assignments, \a false if not.
   */
   inline bool canSMPAssign() const noexcept {
      LeftOperand_<MVM> A( vector_.leftOperand() );
      return ( !BLAZE_BLAS_MODE ||
               !BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION ||
               !BLAZE_BLAS_IS_PARALLEL ||
               ( IsComputation<MT>::value && !evaluateMatrix ) ||
               ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) ) &&
             ( size() > SMP_TDMATDVECMULT_THRESHOLD );
   }
   //**********************************************************************************************

 private:
   //**Member variables****************************************************************************
   LeftOperand  vector_;  //!< Left-hand side dense vector of the multiplication expression.
   RightOperand scalar_;  //!< Right-hand side scalar of the multiplication expression.
   //**********************************************************************************************

   //**Assignment to dense vectors*****************************************************************
   /*!\brief Assignment of a scaled transpose dense matrix-dense vector multiplication to a dense
   //        vector (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a scaled transpose dense
   // matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void assign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL ) {
         return;
      }
      else if( left.columns() == 0UL ) {
         reset( ~lhs );
         return;
      }

      LT A( serial( left  ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( right ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      DVecScalarMultExpr::selectAssignKernel( ~lhs, A, x, rhs.scalar_ );
   }
   //**********************************************************************************************

   //**Assignment to dense vectors (kernel selection)**********************************************
   /*!\brief Selection of the kernel for an assignment of a scaled transpose dense matrix-dense
   //        vector multiplication to a dense vector (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallAssignKernel( y, A, x, scalar );
      else
         selectBlasAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Default assignment to dense vectors*********************************************************
   /*!\brief Default assignment of a scaled transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor
   // \return void
   //
   // This function implements the default assignment kernel for the scaled transpose dense
   // matrix-dense vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectDefaultAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      if( IsStrictlyLower<MT1>::value ) {
         reset( y[0] );
      }

      if( !IsUpper<MT1>::value )
      {
         for( size_t i=( IsStrictlyLower<MT1>::value ? 1UL : 0UL ); i<M; ++i ) {
            y[i] = A(i,0UL) * x[0UL];
         }
      }

      for( size_t j=( IsUpper<MT1>::value && !IsStrictlyUpper<MT1>::value ? 0UL : 1UL ); j<N; ++j )
      {
         if( IsDiagonal<MT1>::value )
         {
            y[j] = A(j,j) * x[j] * scalar;
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? j+1UL : j )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? j-1UL : j )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            const size_t inum( iend - ibegin );
            const size_t ipos( ibegin + ( inum & size_t(-2) ) );

            for( size_t i=ibegin; i<ipos; i+=2UL ) {
               y[i    ] += A(i    ,j) * x[j];
               y[i+1UL] += A(i+1UL,j) * x[j];
            }
            if( ipos < iend ) {
               y[ipos] += A(ipos,j) * x[j];
            }
            if( IsUpper<MT1>::value ) {
               y[iend] = A(iend,j) * x[j];
            }
         }
      }

      if( IsStrictlyUpper<MT1>::value ) {
         reset( y[M-1UL] );
      }

      if( !IsDiagonal<MT1>::value )
      {
         const size_t iend( IsStrictlyUpper<MT1>::value ? M-1UL : M );
         for( size_t i=( IsStrictlyLower<MT1>::value ? 1UL : 0UL ); i<iend; ++i ) {
            y[i] *= scalar;
         }
      }
   }
   //**********************************************************************************************

   //**Default assignment to dense vectors (small matrices)****************************************
   /*!\brief Default assignment of a small scaled transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor
   // \return void
   //
   // This function relays to the default implementation of the assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default assignment to dense vectors (small matrices)*****************************
   /*!\brief Vectorized default assignment of a small scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor
   // \return void
   //
   // This function implements the vectorized default assignment kernel for the scaled transpose
   // dense matrix-dense vector multiplication. This kernel is optimized for small matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      const SIMDType factor( set( scalar ) );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , xmm1*factor );
         y.store( i+SIMDSIZE    , xmm2*factor );
         y.store( i+SIMDSIZE*2UL, xmm3*factor );
         y.store( i+SIMDSIZE*3UL, xmm4*factor );
         y.store( i+SIMDSIZE*4UL, xmm5*factor );
         y.store( i+SIMDSIZE*5UL, xmm6*factor );
         y.store( i+SIMDSIZE*6UL, xmm7*factor );
         y.store( i+SIMDSIZE*7UL, xmm8*factor );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , xmm1*factor );
         y.store( i+SIMDSIZE    , xmm2*factor );
         y.store( i+SIMDSIZE*2UL, xmm3*factor );
         y.store( i+SIMDSIZE*3UL, xmm4*factor );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , xmm1*factor );
         y.store( i+SIMDSIZE    , xmm2*factor );
         y.store( i+SIMDSIZE*2UL, xmm3*factor );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i         ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , xmm1*factor );
         y.store( i+SIMDSIZE, xmm2*factor );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i,j) * x1;
         }

         y.store( i, xmm1*factor );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] = value * scalar;
      }
   }
   //**********************************************************************************************

   //**Default assignment to dense vectors (large matrices)****************************************
   /*!\brief Default assignment of a large scaled transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor
   // \return void
   //
   // This function relays to the default implementation of the assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default assignment to dense vectors (large matrices)*****************************
   /*!\brief Vectorized default assignment of a large scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor
   // \return void
   //
   // This function implements the vectorized default assignment kernel for the scaled transpose
   // dense matrix-dense vector multiplication. This kernel is optimized for large matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      const SIMDType factor( set( scalar ) );

      reset( y );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) + xmm5*factor );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) + xmm6*factor );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) + xmm7*factor );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) + xmm8*factor );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) + xmm1*factor );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) + xmm2*factor );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) + xmm1*factor );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] += value * scalar;
            }
         }
      }
   }
   //**********************************************************************************************

   //**BLAS-based assignment to dense vectors (default)********************************************
   /*!\brief Default assignment of a scaled transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the assignment of a large scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectLargeAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**BLAS-based assignment to dense vectors******************************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*!\brief BLAS-based assignment of a scaled transpose dense matrix-dense vector multiplication
   //        (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function performs the scaled transpose dense matrix-dense vector multiplication based
   // on the according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         assign( y, scalar * x );
         trmv( y, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
      }
      else {
         gemv( y, A, x, ET(scalar), ET(0) );
      }
   }
#endif
   //**********************************************************************************************

   //**Assignment to sparse vectors****************************************************************
   /*!\brief Assignment of a scaled transpose dense matrix-dense vector multiplication to a sparse
   //        vector (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side scaled multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a scaled transpose dense
   // matrix-dense vector multiplication expression to a sparse vector.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline void assign( SparseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      assign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**Addition assignment to dense vectors********************************************************
   /*!\brief Addition assignment of a scaled transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized addition assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void addAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL || left.columns() == 0UL ) {
         return;
      }

      LT A( serial( left  ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( right ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      DVecScalarMultExpr::selectAddAssignKernel( ~lhs, A, x, rhs.scalar_ );
   }
   //**********************************************************************************************

   //**Addition assignment to dense vectors (kernel selection)*************************************
   /*!\brief Selection of the kernel for an addition assignment of a scaled transpose dense
   //        matrix-dense vector multiplication to a dense vector (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallAddAssignKernel( y, A, x, scalar );
      else
         selectBlasAddAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Default addition assignment to dense vectors************************************************
   /*!\brief Default addition assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the default addition assignment kernel for the scaled transpose
   // dense matrix-dense vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectDefaultAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      y.addAssign( A * x * scalar );
   }
   //**********************************************************************************************

   //**Default addition assignment to dense vectors (small matrices)*******************************
   /*!\brief Default addition assignment of a small scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultAddAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default addition assignment to dense vectors (small matrices)********************
   /*!\brief Vectorized default addition assignment of a small scaled transpose dense matrix-dense
   //        vector multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the vectorized default addition assignment kernel for the scaled
   // transpose dense matrix-dense vector multiplication. This kernel is optimized for small
   // matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      const SIMDType factor( set( scalar ) );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , y.load(i             ) + xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
         y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
         y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) + xmm5*factor );
         y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) + xmm6*factor );
         y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) + xmm7*factor );
         y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) + xmm8*factor );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , y.load(i             ) + xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
         y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , y.load(i             ) + xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i         ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , y.load(i         ) + xmm1*factor );
         y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) + xmm2*factor );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1;

         for( size_t j=jbegin; j<jend; ++j ) {
            xmm1 += A.load(i,j) * set( x[j] );
         }

         y.store( i, y.load(i) + xmm1*factor );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] += value * scalar;
      }
   }
   //**********************************************************************************************

   //**Default addition assignment to dense vectors (large matrices)*******************************
   /*!\brief Default addition assignment of a large scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultAddAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default addition assignment to dense vectors (large matrices)********************
   /*!\brief Vectorized default addition assignment of a large scaled transpose dense matrix-dense
   //        vector multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the vectorized default addition assignment kernel for the scaled
   // transpose dense matrix-dense vector multiplication. This kernel is optimized for large
   // matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      const SIMDType factor( set( scalar ) );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) + xmm5*factor );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) + xmm6*factor );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) + xmm7*factor );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) + xmm8*factor );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) + xmm4*factor );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) + xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) + xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) + xmm3*factor );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) + xmm1*factor );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) + xmm2*factor );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) + xmm1*factor );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] += value * scalar;
            }
         }
      }
   }
   //**********************************************************************************************

   //**BLAS-based addition assignment to dense vectors (default)***********************************
   /*!\brief Default addition assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the addition assignment of a large
   // scaled transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectLargeAddAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**BLAS-based addition assignment to dense vectors*********************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*!\brief BLAS-based addition assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function performs the scaled transpose dense matrix-dense vector multiplication based
   // on the according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasAddAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         ResultType_<VT1> tmp( serial( scalar * x ) );
         trmv( tmp, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
         addAssign( y, tmp );
      }
      else {
         gemv( y, A, x, ET(scalar), ET(1) );
      }
   }
#endif
   //**********************************************************************************************

   //**Addition assignment to sparse vectors*******************************************************
   // No special implementation for the addition assignment to sparse vectors.
   //**********************************************************************************************

   //**Subtraction assignment to dense vectors*****************************************************
   /*!\brief Subtraction assignment of a scaled transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized subtraction assignment of a scaled
   // dense transpose matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void subAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL || left.columns() == 0UL ) {
         return;
      }

      LT A( serial( left  ) );  // Evaluation of the left-hand side dense matrix operand
      RT x( serial( right ) );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      DVecScalarMultExpr::selectSubAssignKernel( ~lhs, A, x, rhs.scalar_ );
   }
   //**********************************************************************************************

   //**Subtraction assignment to dense vectors (kernel selection)**********************************
   /*!\brief Selection of the kernel for a subtraction assignment of a scaled transpose dense
   //        matrix-dense vector multiplication to a dense vector (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      if( ( IsDiagonal<MT1>::value ) ||
          ( IsComputation<MT>::value && !evaluateMatrix ) ||
          ( A.rows() * A.columns() < TDMATDVECMULT_THRESHOLD ) )
         selectSmallSubAssignKernel( y, A, x, scalar );
      else
         selectBlasSubAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors*********************************************
   /*!\brief Default subtraction assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the default subtraction assignment kernel for the scaled transpose
   // dense matrix-dense vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline void selectDefaultSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      y.subAssign( A * x * scalar );
   }
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors (small matrices)****************************
   /*!\brief Default subtraction assignment of a small scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the subtraction assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultSubAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default subtraction assignment to dense vectors (small matrices)*****************
   /*!\brief Vectorized default subtraction assignment of a small scaled transpose dense matrix-
   //        dense vector multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the vectorized default subtraction assignment kernel for the
   // scaled transpose dense matrix-dense vector multiplication. This kernel is optimized for
   // small matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectSmallSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t ipos( remainder ? ( M & size_t(-SIMDSIZE) ) : M );
      BLAZE_INTERNAL_ASSERT( !remainder || ( M - ( M % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

      const SIMDType factor( set( scalar ) );

      size_t i( 0UL );

      for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*8UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
            xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
            xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
            xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
            xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
         }

         y.store( i             , y.load(i             ) - xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
         y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4*factor );
         y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) - xmm5*factor );
         y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) - xmm6*factor );
         y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) - xmm7*factor );
         y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) - xmm8*factor );
      }

      for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*4UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3, xmm4;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
            xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
         }

         y.store( i             , y.load(i             ) - xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
         y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4*factor );
      }

      for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*3UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2, xmm3;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i             ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
            xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
         }

         y.store( i             , y.load(i             ) - xmm1*factor );
         y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
         y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
      }

      for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE*2UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1, xmm2;

         for( size_t j=jbegin; j<jend; ++j ) {
            const SIMDType x1( set( x[j] ) );
            xmm1 += A.load(i         ,j) * x1;
            xmm2 += A.load(i+SIMDSIZE,j) * x1;
         }

         y.store( i         , y.load(i         ) - xmm1*factor );
         y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) - xmm2*factor );
      }

      for( ; i<ipos; i+=SIMDSIZE )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+SIMDSIZE, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         SIMDType xmm1;

         for( size_t j=jbegin; j<jend; ++j ) {
            xmm1 += A.load(i,j) * set( x[j] );
         }

         y.store( i, y.load(i) - xmm1*factor );
      }

      for( ; remainder && i<M; ++i )
      {
         const size_t jbegin( ( IsUpper<MT1>::value )
                              ?( IsStrictlyUpper<MT1>::value ? i+1UL : i )
                              :( 0UL ) );
         const size_t jend( ( IsLower<MT1>::value )
                            ?( min( i+1UL, N ) - ( IsStrictlyLower<MT1>::value ? 1UL : 0UL ) )
                            :( N ) );
         BLAZE_INTERNAL_ASSERT( jbegin <= jend, "Invalid loop indices detected" );

         ElementType value{};

         for( size_t j=jbegin; j<jend; ++j ) {
            value += A(i,j) * x[j];
         }

         y[i] -= value * scalar;
      }
   }
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors (large matrices)****************************
   /*!\brief Default subtraction assignment of a large scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the subtraction assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectDefaultSubAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**Vectorized default subtraction assignment to dense vectors (large matrices)*****************
   /*!\brief Vectorized default subtraction assignment of a large scaled transpose dense matrix-
   //        dense vector multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function implements the vectorized default subtraction assignment kernel for the
   // scaled transpose dense matrix-dense vector multiplication. This kernel is optimized for
   // large matrices.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseVectorizedDefaultKernel<VT1,MT1,VT2,ST2> >
      selectLargeSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      constexpr bool remainder( !IsPadded<MT1>::value || !IsPadded<VT1>::value );

      const size_t M( A.rows()    );
      const size_t N( A.columns() );

      const size_t iblock( 32768UL / sizeof( ElementType ) );
      const size_t jblock( ( N < iblock )?( 8UL ):( 4UL ) );

      BLAZE_INTERNAL_ASSERT( ( iblock % SIMDSIZE ) == 0UL, "Invalid block size detected" );

      const SIMDType factor( set( scalar ) );

      for( size_t ii=0U; ii<M; ii+=iblock ) {
         for( size_t jj=0UL; jj<N; jj+=jblock )
         {
            const size_t jend( min( jj+jblock, N ) );
            const size_t itmp( min( ii+iblock, M ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( min( itmp, ( IsStrictlyUpper<MT1>::value ? jend-1UL : jend ) ) )
                               :( itmp ) );

            const size_t ipos( remainder ? ( iend & size_t(-SIMDSIZE) ) : iend );
            BLAZE_INTERNAL_ASSERT( !remainder || ( iend - ( iend % SIMDSIZE ) ) == ipos, "Invalid end calculation" );

            size_t i( ( IsLower<MT1>::value )
                      ?( max( ii, ( IsStrictlyLower<MT1>::value ? jj+1UL : jj ) & size_t(-SIMDSIZE) ) )
                      :( ii ) );

            for( ; (i+SIMDSIZE*7UL) < ipos; i+=SIMDSIZE*8UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
                  xmm5 += A.load(i+SIMDSIZE*4UL,j) * x1;
                  xmm6 += A.load(i+SIMDSIZE*5UL,j) * x1;
                  xmm7 += A.load(i+SIMDSIZE*6UL,j) * x1;
                  xmm8 += A.load(i+SIMDSIZE*7UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4*factor );
               y.store( i+SIMDSIZE*4UL, y.load(i+SIMDSIZE*4UL) - xmm5*factor );
               y.store( i+SIMDSIZE*5UL, y.load(i+SIMDSIZE*5UL) - xmm6*factor );
               y.store( i+SIMDSIZE*6UL, y.load(i+SIMDSIZE*6UL) - xmm7*factor );
               y.store( i+SIMDSIZE*7UL, y.load(i+SIMDSIZE*7UL) - xmm8*factor );
            }

            for( ; (i+SIMDSIZE*3UL) < ipos; i+=SIMDSIZE*4UL )
            {
               SIMDType xmm1, xmm2, xmm3, xmm4;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
                  xmm4 += A.load(i+SIMDSIZE*3UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
               y.store( i+SIMDSIZE*3UL, y.load(i+SIMDSIZE*3UL) - xmm4*factor );
            }

            for( ; (i+SIMDSIZE*2UL) < ipos; i+=SIMDSIZE*3UL )
            {
               SIMDType xmm1, xmm2, xmm3;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i             ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE    ,j) * x1;
                  xmm3 += A.load(i+SIMDSIZE*2UL,j) * x1;
               }

               y.store( i             , y.load(i             ) - xmm1*factor );
               y.store( i+SIMDSIZE    , y.load(i+SIMDSIZE    ) - xmm2*factor );
               y.store( i+SIMDSIZE*2UL, y.load(i+SIMDSIZE*2UL) - xmm3*factor );
            }

            for( ; (i+SIMDSIZE) < ipos; i+=SIMDSIZE*2UL )
            {
               SIMDType xmm1, xmm2;

               for( size_t j=jj; j<jend; ++j ) {
                  const SIMDType x1( set( x[j] ) );
                  xmm1 += A.load(i         ,j) * x1;
                  xmm2 += A.load(i+SIMDSIZE,j) * x1;
               }

               y.store( i         , y.load(i         ) - xmm1*factor );
               y.store( i+SIMDSIZE, y.load(i+SIMDSIZE) - xmm2*factor );
            }

            for( ; i<ipos; i+=SIMDSIZE )
            {
               SIMDType xmm1;

               for( size_t j=jj; j<jend; ++j ) {
                  xmm1 += A.load(i,j) * set( x[j] );
               }

               y.store( i, y.load(i) - xmm1*factor );
            }

            for( ; remainder && i<iend; ++i )
            {
               ElementType value{};

               for( size_t j=jj; j<jend; ++j ) {
                  value += A(i,j) * x[j];
               }

               y[i] -= value * scalar;
            }
         }
      }
   }
   //**********************************************************************************************

   //**BLAS-based subtraction assignment to dense vectors (default)********************************
   /*!\brief Default subtraction assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function relays to the default implementation of the subtraction assignment of a large
   // scaled transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline DisableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      selectLargeSubAssignKernel( y, A, x, scalar );
   }
   //**********************************************************************************************

   //**BLAS-based subtraction assignment to dense vectors******************************************
#if BLAZE_BLAS_MODE && BLAZE_USE_BLAS_MATRIX_VECTOR_MULTIPLICATION
   /*!\brief BLAS-based subtraction assignment of a scaled transpose dense matrix-dense vector
   //        multiplication (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side dense vector operand.
   // \param scalar The scaling factor.
   // \return void
   //
   // This function performs the scaled transpose dense matrix-dense vector multiplication based
   // on the according BLAS functionality.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2    // Type of the right-hand side vector operand
           , typename ST2 >  // Type of the scalar value
   static inline EnableIf_< UseBlasKernel<VT1,MT1,VT2,ST2> >
      selectBlasSubAssignKernel( VT1& y, const MT1& A, const VT2& x, ST2 scalar )
   {
      using ET = ElementType_<VT1>;

      if( IsTriangular<MT1>::value ) {
         ResultType_<VT1> tmp( serial( scalar * x ) );
         trmv( tmp, A, ( IsLower<MT1>::value )?( CblasLower ):( CblasUpper ) );
         subAssign( y, tmp );
      }
      else {
         gemv( y, A, x, ET(-scalar), ET(1) );
      }
   }
#endif
   //**********************************************************************************************

   //**Subtraction assignment to sparse vectors****************************************************
   // No special implementation for the subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**Multiplication assignment to dense vectors**************************************************
   /*!\brief Multiplication assignment of a scaled transpose dense matrix-dense vector
   //        multiplication to a dense vector (\f$ \vec{y}*=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized multiplication assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void multAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      multAssign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**Multiplication assignment to sparse vectors*************************************************
   // No special implementation for the multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**Division assignment to dense vectors********************************************************
   /*!\brief Division assignment of a scaled transpose dense matrix-dense vector multiplication to
   //        a dense vector (\f$ \vec{y}/=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression divisor.
   // \return void
   //
   // This function implements the performance optimized division assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void divAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      divAssign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**Division assignment to sparse vectors*******************************************************
   // No special implementation for the division assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP assignment to dense vectors*************************************************************
   /*!\brief SMP assignment of a scaled transpose dense matrix-dense vector multiplication to a
   //        dense vector (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL ) {
         return;
      }
      else if( left.columns() == 0UL ) {
         reset( ~lhs );
         return;
      }

      LT A( left  );  // Evaluation of the left-hand side dense matrix operand
      RT x( right );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      smpAssign( ~lhs, A * x * rhs.scalar_ );
   }
   //**********************************************************************************************

   //**SMP assignment to sparse vectors************************************************************
   /*!\brief SMP assignment of a scaled transpose dense matrix-dense vector multiplication to a
   //        sparse vector (\f$ \vec{y}=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side scaled multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a scaled transpose
   // dense matrix-dense vector multiplication expression to a sparse vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAssign( SparseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpAssign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**SMP addition assignment to dense vectors****************************************************
   /*!\brief SMP addition assignment of a scaled transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}+=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized SMP addition assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector. Due to
   // the explicit application of the SFINAE principle, this function can only be selected by
   // the compiler in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpAddAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL || left.columns() == 0UL ) {
         return;
      }

      LT A( left  );  // Evaluation of the left-hand side dense matrix operand
      RT x( right );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      smpAddAssign( ~lhs, A * x * rhs.scalar_ );
   }
   //**********************************************************************************************

   //**SMP addition assignment to sparse vectors***************************************************
   // No special implementation for the SMP addition assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP subtraction assignment to dense vectors*************************************************
   /*!\brief SMP subtraction assignment of a scaled transpose dense matrix-dense vector
   //        multiplication to a dense vector (\f$ \vec{y}-=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized SMP subtraction assignment of a scaled
   // dense transpose matrix-dense vector multiplication expression to a dense vector. Due to
   // the explicit application of the SFINAE principle, this function can only be selected by
   // the compiler in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpSubAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      LeftOperand_<MVM>  left ( rhs.vector_.leftOperand()  );
      RightOperand_<MVM> right( rhs.vector_.rightOperand() );

      if( left.rows() == 0UL || left.columns() == 0UL ) {
         return;
      }

      LT A( left  );  // Evaluation of the left-hand side dense matrix operand
      RT x( right );  // Evaluation of the right-hand side dense vector operand

      BLAZE_INTERNAL_ASSERT( A.rows()    == left.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == left.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == right.size()  , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size() , "Invalid vector size"       );

      smpSubAssign( ~lhs, A * x * rhs.scalar_ );
   }
   //**********************************************************************************************

   //**SMP subtraction assignment to sparse vectors************************************************
   // No special implementation for the SMP subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP multiplication assignment to dense vectors**********************************************
   /*!\brief SMP multiplication assignment of a scaled transpose dense matrix-dense vector
   //        multiplication to a dense vector (\f$ \vec{y}*=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized SMP multiplication assignment of a
   // scaled transpose dense matrix-dense vector multiplication expression to a dense vector.
   // Due to the explicit application of the SFINAE principle, this function can only be
   // selected by the compiler in case the expression specific parallel evaluation strategy
   // is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpMultAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpMultAssign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**SMP multiplication assignment to sparse vectors*********************************************
   // No special implementation for the SMP multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP division assignment to dense vectors****************************************************
   /*!\brief SMP division assignment of a scaled transpose dense matrix-dense vector multiplication
   //        to a dense vector (\f$ \vec{y}/=s*A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side scaled multiplication expression divisor.
   // \return void
   //
   // This function implements the performance optimized SMP division assignment of a scaled
   // transpose dense matrix-dense vector multiplication expression to a dense vector. Due to
   // the explicit application of the SFINAE principle, this function can only be selected by
   // the compiler in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline EnableIf_< UseSMPAssign<VT1> >
      smpDivAssign( DenseVector<VT1,false>& lhs, const DVecScalarMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_NOT_REQUIRE_EVALUATION( ResultType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpDivAssign( ~lhs, tmp );
   }
   //**********************************************************************************************

   //**SMP division assignment to sparse vectors***************************************************
   // No special implementation for the SMP division assignment to sparse vectors.
   //**********************************************************************************************

   //**Compile time checks*************************************************************************
   BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( MVM );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( MVM );
   BLAZE_CONSTRAINT_MUST_BE_DENSE_MATRIX_TYPE ( MT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_MAJOR_MATRIX_TYPE( MT );
   BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( VT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( VT );
   BLAZE_CONSTRAINT_MUST_BE_NUMERIC_TYPE( ST );
   BLAZE_CONSTRAINT_MUST_BE_SAME_TYPE( ST, RightOperand );
   //**********************************************************************************************
};
/*! \endcond */
//*************************************************************************************************




//=================================================================================================
//
//  GLOBAL BINARY ARITHMETIC OPERATORS
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Multiplication operator for the multiplication of a column-major dense matrix and a dense
//        vector (\f$ \vec{y}=A*\vec{x} \f$).
// \ingroup dense_vector
//
// \param mat The left-hand side column-major dense matrix for the multiplication.
// \param vec The right-hand side dense vector for the multiplication.
// \return The resulting vector.
// \exception std::invalid_argument Matrix and vector sizes do not match.
//
// This operator represents the multiplication between a column-major dense matrix and a dense
// vector:

   \code
   using blaze::columnMajor;
   using blaze::columnVector;

   blaze::DynamicMatrix<double,columnMajor> A;
   blaze::DynamicVector<double,columnVector> x, y;
   // ... Resizing and initialization
   y = A * x;
   \endcode

// The operator returns an expression representing a dense vector of the higher-order element
// type of the two involved element types \a MT::ElementType and \a VT::ElementType. Both the
// dense matrix type \a MT and the dense vector type \a VT as well as the two element types
// \a MT::ElementType and \a VT::ElementType have to be supported by the MultTrait class
// template.\n
// In case the current size of the vector \a vec doesn't match the current number of columns
// of the matrix \a mat, a \a std::invalid_argument is thrown.
*/
template< typename MT    // Type of the left-hand side dense matrix
        , typename VT >  // Type of the right-hand side dense vector
inline decltype(auto)
   operator*( const DenseMatrix<MT,true>& mat, const DenseVector<VT,false>& vec )
{
   BLAZE_FUNCTION_TRACE;

   BLAZE_CONSTRAINT_MUST_NOT_BE_MATMATMULTEXPR_TYPE( MT );

   if( (~mat).columns() != (~vec).size() ) {
      BLAZE_THROW_INVALID_ARGUMENT( "Matrix and vector sizes do not match" );
   }

   using ReturnType = const TDMatDVecMultExpr<MT,VT>;
   return ReturnType( ~mat, ~vec );
}
//*************************************************************************************************




//=================================================================================================
//
//  SIZE SPECIALIZATIONS
//
//=================================================================================================

//*************************************************************************************************
/*! \cond BLAZE_INTERNAL */
template< typename MT, typename VT >
struct Size< TDMatDVecMultExpr<MT,VT>, 0UL >
   : public Size<MT,0UL>
{};
/*! \endcond */
//*************************************************************************************************




//=================================================================================================
//
//  ISALIGNED SPECIALIZATIONS
//
//=================================================================================================

//*************************************************************************************************
/*! \cond BLAZE_INTERNAL */
template< typename MT, typename VT >
struct IsAligned< TDMatDVecMultExpr<MT,VT> >
   : public And< IsAligned<MT>, IsAligned<VT> >
{};
/*! \endcond */
//*************************************************************************************************

} // namespace blaze

#endif
