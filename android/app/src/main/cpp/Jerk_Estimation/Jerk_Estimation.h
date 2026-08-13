//
// File: Jerk_Estimation.h
//
// Code generated for Simulink model 'Jerk_Estimation'.
//
// Model version                  : 1.12
// Simulink Coder version         : 24.2 (R2024b) 21-Jun-2024
// C/C++ source code generated on : Tue Jan 28 16:35:10 2025
//
// Target selection: ert.tlc
// Embedded hardware selection: ARM Compatible->ARM 10
// Code generation objectives:
//    1. Execution efficiency
//    2. RAM efficiency
// Validation result: Not run
//
// References:
//    1. A simple improved velocity estimation for low-speed regions based on position measurements only. https://doi.org/10.1109/TCST.2006.876917
//    2. A New Inertial Aid Method for High Dynamic Compass Signal Tracking Based on a Nonlinear Tracking Differentiator. https://doi.org/10.3390/s120607634
//
#ifndef Jerk_Estimation_h_
#define Jerk_Estimation_h_
#include <cmath>
#include "rtwtypes.h"

// Class declaration for model Jerk_Estimation
class Jerk_Estimation final
{
  // public data and function members
 public:
  // Block signals and states (default storage) for system '<Root>'
  struct DW {
    real_T fal1;                       // '<S1>/fal1'
    real_T fal;                        // '<S1>/fal'
    real_T Memory_PreviousInput;       // '<Root>/Memory'
    real_T Memory1_PreviousInput;      // '<Root>/Memory1'
  };

  // External inputs (root inport signals with default storage)
  struct ExtU {
    real_T acceleration;               // '<Root>/acceleration'
    real_T tor;                        // '<Root>/tor'
  };

  // External outputs (root outports fed by signals with default storage)
  struct ExtY {
    real_T jerk;                       // '<Root>/jerk'
  };

  // Copy Constructor
  Jerk_Estimation(Jerk_Estimation const&) = delete;

  // Assignment Operator
  Jerk_Estimation& operator= (Jerk_Estimation const&) & = delete;

  // Move Constructor
  Jerk_Estimation(Jerk_Estimation &&) = delete;

  // Move Assignment Operator
  Jerk_Estimation& operator= (Jerk_Estimation &&) = delete;

  // External inputs
  ExtU rtU;

  // External outputs
  ExtY rtY;

  // model initialize function
  static void initialize();

  // model step function
  void step();

  // Constructor
  Jerk_Estimation();

  // Destructor
  ~Jerk_Estimation();

  // private data and function members
 private:
  // Block states
  DW rtDW;
};

//-
//  These blocks were eliminated from the model due to optimizations:
//
//  Block '<S1>/Scope' : Unused code path elimination


//-
//  The generated code includes comments that allow you to trace directly
//  back to the appropriate location in the model.  The basic format
//  is <system>/block_name, where system is the system number (uniquely
//  assigned by Simulink) and block_name is the name of the block.
//
//  Use the MATLAB hilite_system command to trace the generated code back
//  to the model.  For example,
//
//  hilite_system('<S3>')    - opens system 3
//  hilite_system('<S3>/Kp') - opens and selects block Kp which resides in S3
//
//  Here is the system hierarchy for this model
//
//  '<Root>' : 'Jerk_Estimation'
//  '<S1>'   : 'Jerk_Estimation/Nonlinear Function'

#endif                                 // Jerk_Estimation_h_

//
// File trailer for generated code.
//
// [EOF]
//
