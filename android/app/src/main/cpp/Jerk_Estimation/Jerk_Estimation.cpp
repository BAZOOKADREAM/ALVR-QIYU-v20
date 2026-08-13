//
// File: Jerk_Estimation.cpp
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
#include "Jerk_Estimation.h"
#include "rtwtypes.h"

// Model step function
void Jerk_Estimation::step()
{
  real_T rtb_Add2;
  real_T rtb_Memory;
  real_T rtb_Product1;
  real_T rtb_Product2;

  // Memory: '<Root>/Memory'
  rtb_Memory = rtDW.Memory_PreviousInput;

  // CFunction: '<S1>/fal1' incorporates:
  //   Constant: '<S1>/Constant3'
  //   Constant: '<S1>/Constant7'
  //   Constant: '<S1>/Constant8'
  //   Product: '<S1>/Product3'

  if (abs((rtb_Memory / 2.0)) > (0.01)) {
    rtDW.fal1 = pow(abs((rtb_Memory / 2.0)), (0.66666666666666663)) *
      (((rtb_Memory / 2.0) > 0.0) ? 1 : -1);
  } else {
    rtDW.fal1 = (rtb_Memory / 2.0) * pow((0.01), (0.66666666666666663) - 1);
  }

  // Product: '<S1>/Product2' incorporates:
  //   Constant: '<S1>/Constant6'

  rtb_Product2 = 60.0 * rtDW.fal1;

  // Sum: '<Root>/Add2' incorporates:
  //   Inport: '<Root>/acceleration'
  //   Memory: '<Root>/Memory1'

  rtb_Add2 = rtDW.Memory1_PreviousInput - rtU.acceleration;

  // CFunction: '<S1>/fal' incorporates:
  //   Constant: '<S1>/Constant4'
  //   Constant: '<S1>/Constant5'

  if (abs(rtb_Add2) > (0.01)) {
    rtDW.fal = pow(abs(rtb_Add2), (0.66666666666666663)) * ((rtb_Add2 > 0.0) ? 1
      : -1);
  } else {
    rtDW.fal = rtb_Add2 * pow((0.01), (0.66666666666666663) - 1);
  }

  // Product: '<S1>/Product1' incorporates:
  //   Constant: '<S1>/Constant2'

  rtb_Product1 = 60.0 * rtDW.fal;

  // Sum: '<Root>/Add' incorporates:
  //   Constant: '<S1>/Constant'
  //   Constant: '<S1>/Constant1'
  //   Inport: '<Root>/tor'
  //   Product: '<Root>/Product'
  //   Product: '<S1>/Product'
  //   Product: '<S1>/Product4'
  //   Sum: '<S1>/Add'

  rtb_Product2 = ((60.0 * rtb_Add2 + rtb_Product1) + rtb_Product2) * -4.0 *
    rtU.tor + rtb_Memory;

  // Outport: '<Root>/jerk'
  rtY.jerk = rtb_Product2;

  // Update for Memory: '<Root>/Memory'
  rtDW.Memory_PreviousInput = rtb_Product2;

  // Update for Memory: '<Root>/Memory1' incorporates:
  //   Inport: '<Root>/tor'
  //   Product: '<Root>/Product1'
  //   Sum: '<Root>/Add1'

  rtDW.Memory1_PreviousInput += rtU.tor * rtb_Memory;
}

// Model initialize function
void Jerk_Estimation::initialize()
{
  // (no initialization code required)
}

// Constructor
Jerk_Estimation::Jerk_Estimation():
  rtU(),
  rtY(),
  rtDW()
{
  // Currently there is no constructor body generated.
}

// Destructor
// Currently there is no destructor body generated.
Jerk_Estimation::~Jerk_Estimation() = default;

//
// File trailer for generated code.
//
// [EOF]
//
