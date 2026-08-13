#include "Hand_Trajectory_Prediction.h"

// regression coefficients of the prediction-time dependent kinematics model
double a1(double t) {
    if (t < 0.16) {
        return 1.0000 + 0.1693 * t;
    } else {
        return 1.0174 + 0.4547 * t - 2.4655 * t * t;
    }
}

double a2(double t) {
    if (t < 0.16) {
        return 0.5000 + 0.1837 * t;
    } else {
        return 0.6550 - 0.7458 * t - 0.2458 * t * t;
    }
}

double a3(double t) {
    if (t < 0.16) {
        return 0.1667 + 0.1151 * t;
    } else {
        return 0.2637 - 0.5122 * t + 0.1308 * t * t;
    }
}

double a4(double t) {
    if (t < 0.16) {
        return 0.0417 - 0.0343 * t;
    } else {
        return 0.0739 - 0.2809 * t + 0.2836 * t * t;
    }
}

double a5(double t) {
    if (t < 0.16) {
        return 0.0083 - 0.0064 * t;
    } else {
        return 0.0150 - 0.0569 * t + 0.0555 * t * t;
    }
}

float handTrajectoryPrediction(const handPositionf HandPosition, const double PredictionInSeconds) {
    float PredictedHandPosition = HandPosition.Position;

    if (PredictionInSeconds > 0.0) {
        PredictedHandPosition += 
            HandPosition.LinearVelocity * PredictionInSeconds * a1(PredictionInSeconds) +
            HandPosition.LinearAcceleration * PredictionInSeconds * PredictionInSeconds * a2(PredictionInSeconds) +
            HandPosition.LinearJerk * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * a3(PredictionInSeconds) +
            HandPosition.LinearSnap * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * a4(PredictionInSeconds) +
            HandPosition.LinearCrackle * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * PredictionInSeconds * a5(PredictionInSeconds);
    }

    return PredictedHandPosition;
}