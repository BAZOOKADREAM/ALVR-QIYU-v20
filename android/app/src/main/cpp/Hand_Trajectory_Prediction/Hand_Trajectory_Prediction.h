#ifndef Hand_Trajectory_Prediction_H
#define Hand_Trajectory_Prediction_H

// clang-format off
/* @mainpage Hand Trajectory Prediction
 *
 * @cite So Predictable! Continuous 3D Hand Trajectory Prediction in Virtual Reality. https://doi.org/10.1145/3472749.3474753
 * 
 * The predictive model uses classical kinematic equations as the base of a multi-layer regressive model.
 * 
 * The displacement of the hand can be described by the following kinematic equations:
 * D(t) = V(t) * t + 1/2 * A(t) * t^2 + 1/6 * J(t) * t^3 + 1/24 * S(t) * t^4 + 1/120 * C(t) * t^5
 * where D(t) is the displacement of the hand at time t, V(t) is the velocity, A(t) is the acceleration, J(t) is the jerk, S(t) is the snap, and C(t) is the crackle.
 * 
 * The prediction-time dependent kinematics regression is used to predict the hand position at a future time t0 + t.
 * D(t) = V(t0) * t * a1(t) + A(t0) * t^2 * a2(t) + J(t0) * t^3 * a3(t) + S(t0) * t^4 * a4(t) + C(t0) * t^5 * a5(t)
 * where a1(t), a2(t), a3(t), a4(t), and a5(t) are the regression coefficients.
 * 
 * The model is trained using a dataset of hand trajectories collected from a user study.
 * Therefore, the regression coefficients of the model can be expressed as:
 * a1(t) = 1.0000 + 0.1693 * t (t < 0.16s) or 1.0174 + 0.4547 * t - 2.4655 * t^2 (t >= 0.16s)
 * a2(t) = 0.5000 + 0.1837 * t (t < 0.16s) or 0.6550 - 0.7458 * t - 0.2458 * t^2 (t >= 0.16s)
 * a3(t) = 0.1667 + 0.1151 * t (t < 0.16s) or 0.2637 - 0.5122 * t + 0.1308 * t^2 (t >= 0.16s)
 * a4(t) = 0.0417 - 0.0343 * t (t < 0.16s) or 0.0739 - 0.2809 * t + 0.2836 * t^2 (t >= 0.16s)
 * a5(t) = 0.0083 - 0.0064 * t (t < 0.16s) or 0.0150 - 0.0569 * t + 0.0555 * t^2 (t >= 0.16s)
*/
// clang-format on

#if defined(__cplusplus)
extern "C" {
#endif

// Structure to hold the hand position and its kinematic properties.
typedef struct handPositionf_ {
    float Position;
    float LinearVelocity;
    float LinearAcceleration;
    float LinearJerk;
    float LinearSnap;
    float LinearCrackle;
} handPositionf;

/* @brief Predicts the hand trajectory based on the current hand position.
 * @param[in] HandPosition The current hand position.
 * @param[in] PredictionInSeconds The seconds this position was predicted ahead.
 * @return The predicted hand position.
 * @note The prediction time is in seconds.
 */
float handTrajectoryPrediction(
    const handPositionf HandPosition,
    const double PredictionInSeconds
    );

#if defined(__cplusplus)
} // extern "C"
#endif

#endif//Hand_Trajectory_Prediction_H