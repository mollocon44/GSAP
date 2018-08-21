// Copyright (c) 2018 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.

#include <stdio.h>

#include "ExtendedKalmanFilter.h"


/**
 Functions are still largely copied from UKF, header file being designed first and waiting on decision for how to deal with Jacobians.
**/

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Exceptions.h"
#include "ConfigMap.h"
//#include "Observers/ExtendedKalmanFilter.h"
#include "ThreadSafeLog.h"
#include "UData.h"

namespace PCOE {
    const static Log& log = Log::Instance();
    
    // Configuration Keys
    const std::string Q_KEY = "Observer.Q";
    const std::string R_KEY = "Observer.R";

    // Other string constants
    const std::string MODULE_NAME = "OBS-EKF";
    
    
    ExtendedKalmanFilter::ExtendedKalmanFilter(const Model* m) : Observer(m) {
        Expect(m != nullptr, "Invalid model");
        xEstimated = model->getStateVector();
        uPrev = model->getInputVector();
        zEstimated = model->getOutputVector();
    }
    
    ExtendedKalmanFilter::ExtendedKalmanFilter(const Model* m, Matrix q, Matrix r)
    : ExtendedKalmanFilter(m) {
        Expect(q.rows() == q.cols(), "q is not square");
        Expect(q.rows() == model->getStateSize(), "Size of q does not match model state size");
        Expect(r.rows() == r.cols(), "q is not square");
        Expect(r.rows() == model->getOutputSize(), "Size of r does not match model output size");
        
        Q = std::move(q);
        R = std::move(r);
    }
    
    // ConfigMap-based Constructor
    ExtendedKalmanFilter::ExtendedKalmanFilter(const Model* model, const ConfigMap& config)
    : ExtendedKalmanFilter(model) {
        requireKeys(config, {Q_KEY, R_KEY});
        
        // Set Q
        log.WriteLine(LOG_TRACE, MODULE_NAME, "Setting Q");
        auto& QValues = config.getVector(Q_KEY);
        Require(std::abs(std::sqrt(QValues.size()) - std::floor(std::sqrt(QValues.size()))) < 1e-12,
                "Q values can not describe a square matrix");
        auto qDim = static_cast<std::size_t>(sqrt(QValues.size()));
        Q.resize(qDim, qDim);
        std::size_t qValueIndex = 0;
        for (std::size_t row = 0; row < qDim; row++) {
            for (std::size_t col = 0; col < qDim; col++) {
                Q[row][col] = std::stod(QValues[qValueIndex]);
                qValueIndex++;
            }
        }
        
        // Set R
        log.WriteLine(LOG_TRACE, MODULE_NAME, "Setting R");
        auto& RValues = config.getVector(R_KEY);
        Require(std::abs(std::sqrt(RValues.size()) - std::floor(std::sqrt(RValues.size()))) < 1e-12,
                "R values can not describe a square matrix");
        auto rDim = static_cast<std::size_t>(sqrt(RValues.size()));
        R.resize(rDim, rDim);
        std::size_t rValueIndex = 0;
        for (std::size_t row = 0; row < rDim; row++) {
            for (std::size_t col = 0; col < rDim; col++) {
                R[row][col] = std::stod(RValues[rValueIndex]);
                rValueIndex++;
            }
        }
        
        log.WriteLine(LOG_INFO, MODULE_NAME, "Created EKF");
    }
    
    
    // Tentatively OK for EKF
    void ExtendedKalmanFilter::initialize(const double t0,
                                           const Model::state_type& x0,
                                           const Model::input_type& u0) {
        log.WriteLine(LOG_DEBUG, MODULE_NAME, "Initializing");
        
        // Initialize time, state, inputs
        lastTime = t0;
        xEstimated = x0;
        uPrev = u0;
        
        // Initialize P
        P = Q;
        
        // Compute corresponding output estimate
        std::vector<double> zeroNoiseZ(model->getOutputSize());
        zEstimated = model->outputEqn(lastTime, xEstimated, uPrev, zeroNoiseZ);
        
        // Set initialized flag
        initialized = true;
        log.WriteLine(LOG_DEBUG, MODULE_NAME, "Initialize completed");
    }
    
    void ExtendedKalmanFilter::step(const double timestamp,
                                     const Model::input_type& u,
                                     const Model::output_type& z) {
        log.WriteLine(LOG_DEBUG, MODULE_NAME, "Starting step");
        Expect(isInitialized(), "Not initialized");
        Expect(timestamp - lastTime > 0, "Time has not advanced");
        
        // Update time
        double dt = timestamp - lastTime;
        lastTime = timestamp;
        
        std::size_t sigmaPointCount = sigmaX.M.cols();
        
        // 1. Predict
        log.WriteLine(LOG_TRACE, MODULE_NAME, "Starting step - predict");
        
        // xkk1 = f(k1k1,uPrev) //calc next state from model assuming no noise
        // ykk1 = h(xkk1) //calc next expected sensor readings from expected state
        // F = jacobian(xkk1,ykk1) //get jacobian eval'd at new expected state, sensor output
        // Update Pkk1 using F,Pk1k1,Q
        // H = jacobian(xkk1,
        
        
        
        // 2. Update
        log.WriteLine(LOG_TRACE, MODULE_NAME, "Starting step - update");
        
        // Compute state-output cross-covariance matrix
        Matrix Pxz(model->getStateSize(), model->getOutputSize());
        for (unsigned int i = 0; i < sigmaPointCount; i++) {
            Matrix columnx(model->getStateSize(), 1);
            Matrix columnz(model->getOutputSize(), 1);
            columnx.col(0, Xkk1.col(i));
            columnz.col(0, Zkk1.col(i));
            
            Matrix xkk1m(model->getStateSize(), 1);
            Matrix zkk1m(model->getOutputSize(), 1);
            xkk1m.col(0, xkk1);
            zkk1m.col(0, zkk1);
            
            Matrix diffx = columnx - xkk1m;
            Matrix diffzT = (columnz - zkk1m).transpose();
            Matrix temp = diffx * diffzT;
            Pxz = Pxz + temp * sigmaX.w[i];
        }
        
        // Compute Kalman gain
        Matrix Kk = Pxz * Pzz.inverse();
        
        // Compute state estimate
        Matrix xkk1m(model->getStateSize(), 1);
        Matrix zkk1m(model->getOutputSize(), 1);
        Matrix zm(model->getOutputSize(), 1);
        xkk1m.col(0, xkk1);
        zkk1m.col(0, zkk1);
        zm.col(0, z.vec());
        Matrix xk1m = xkk1m + Kk * (zm - zkk1m);
        xEstimated = Model::state_type(static_cast<std::vector<double>>(xk1m.col(0)));
        
        // Compute output estimate
        std::vector<double> zeroNoiseZ(model->getOutputSize());
        zEstimated = model->outputEqn(timestamp, xEstimated, u, zeroNoiseZ);
        
        // Compute covariance
        P = Pkk1 - Kk * Pzz * Kk.transpose();
        
        // Update uOld
        uPrev = u;
    }
    
    /* EMPTY JACOBIAN PLACEHOLDER */
    const Matrix ExtendedKalmanFilter::jacobian(const Model::state_type& mx, const Model::input_type& u) {
        //return an empty matrix until I've figured out inputs/outputs
        Matrix j
        return j
    }
    
    
    
    
    
    
    /*void ExtendedKalmanFilter::computeSigmaPoints(const Model::state_type& mx,
                                                   const Matrix& Pxx,
                                                   SigmaPoints& sigma) {
        log.WriteLine(LOG_TRACE, MODULE_NAME, "Computing sigma points");
        
        // Assumes that sigma points have been set up correctly within the constructor
        auto stateSize = mx.size();
        auto sigmaPointCount = sigma.M.cols();
        
        // First sigma point is the mean
        for (unsigned int i = 0; i < stateSize; i++) {
            sigma.M[i][0] = mx[i];
        }
        
        // Compute a matrix square root using Cholesky decomposition
        Matrix nkPxx = Pxx;
        for (unsigned int i = 0; i < nkPxx.rows(); i++) {
            for (unsigned int j = 0; j < nkPxx.cols(); j++) {
                nkPxx[i][j] *= (stateSize + sigma.kappa);
            }
        }
        Matrix matrixSq = nkPxx.chol();
        
        // For sigma points 2 to n+1, it is mx + ith column of matrix square root
        for (unsigned int i = 0; i < stateSize; i++) {
            // Set column
            for (unsigned int j = 0; j < stateSize; j++) {
                sigma.M[i][j + 1] = mx[i] + matrixSq[i][j];
            }
        }
        
        // For sigma points n+2 to 2n+1, it is mx - ith column of matrix square root
        for (unsigned int i = 0; i < stateSize; i++) {
            // Set column
            for (unsigned int j = 0; j < stateSize; j++) {
                sigma.M[i][j + stateSize + 1] = mx[i] - matrixSq[i][j];
            }
        }
        
        // w(1) is kappa/(n+kappa)
        sigma.w[0] = sigma.kappa / (stateSize + sigma.kappa);
        
        // Rest of w are 0.5/(n+kappa)
        for (unsigned int i = 1; i < sigmaPointCount; i++) {
            sigma.w[i] = 0.5 / (stateSize + sigma.kappa);
        }
        
        // Scale the sigma points
        // 1. Xi' = X0 + alpha*(Xi-X0)
        Matrix X0 = sigma.M.col(0);
        for (unsigned int i = 1; i < sigmaPointCount; i++) {
            sigma.M.col(i, X0 + sigma.alpha * (sigma.M.col(i) - X0));
        }
        
        // 2. W0' = W0/alpha^2 + (1/alpha^2-1)
        //    Wi' = Wi/alpha^2
        sigma.w[0] = sigma.w[0] / sigma.alpha / sigma.alpha + (1 / sigma.alpha / sigma.alpha - 1);
        for (unsigned int i = 1; i < sigmaPointCount; i++) {
            sigma.w[i] = sigma.w[i] / sigma.alpha / sigma.alpha;
        }
    }*/
    
    std::vector<UData> UnscentedKalmanFilter::getStateEstimate() const {
        std::vector<UData> state(model->getStateSize());
        for (unsigned int i = 0; i < model->getStateSize(); i++) {
            state[i].uncertainty(UType::MeanCovar);
            state[i].npoints(model->getStateSize());
            state[i][MEAN] = xEstimated[i];
            state[i][COVAR()] = static_cast<std::vector<double>>(P.row(i));
        }
        return state;
    }
}
