#include "Phobri64.h"
#include "curve_fitting.h"

float linearize(const float point, const float coefficients[]) {
	return (coefficients[0]*(point*point*point) + coefficients[1]*(point*point) + coefficients[2]*point + coefficients[3]);
}

/*
 * calcStickValues computes the stick x/y coordinates from angle.
 * This requires weird trig because the stick moves spherically.
 */
void calc_stick_values(float angle, float* x, float* y) {
	*x = 100*atan2f((sinf(MAX_STICK_ANGLE)*cosf(angle)),cosf(MAX_STICK_ANGLE))/MAX_STICK_ANGLE;
	*y = 100*atan2f((sinf(MAX_STICK_ANGLE)*sinf(angle)),cosf(MAX_STICK_ANGLE))/MAX_STICK_ANGLE;
}

/*
 * Convert the x/y coordinates (actually angles on a sphere) to an azimuth
 * We first convert to a 3D coordinate and then drop to 2D, then arctan it
 * This does the opposite of calcStickValues, ideally.
 */
float angle_on_sphere(const float x, const float y) {
	float xx = sinf(x * MAX_STICK_ANGLE/100) * cosf(y * MAX_STICK_ANGLE/100);
	float yy = cosf(x * MAX_STICK_ANGLE/100) * sinf(y * MAX_STICK_ANGLE/100);
	float angle = atan2f(yy, xx); // WHY IS THIS BACKWARDS
	if(angle < 0){
		angle += 2*M_PI;
	}

    return angle;
}


void clean_cal_points(const float raw_cal_points_x[], const float raw_cal_points_y[], float cleaned_points_x[], float cleaned_points_y[]) {
	cleaned_points_x[0] = 0;
	cleaned_points_y[0] = 0;

	for (int i = 0; i < NUM_NOTCHES; i++) {
		// each origin reading is summed together
		cleaned_points_x[0] += raw_cal_points_x[i*2];
		cleaned_points_y[0] += raw_cal_points_y[i*2];

		// for the notches, copy point into cleaned list
		cleaned_points_x[i+1] = raw_cal_points_x[i*2+1];
		cleaned_points_y[i+1] = raw_cal_points_y[i*2+1];
	}

	// remove the largest and smallest two origin values to remove outliers
	// first, find their indices

	int smallestX = 0;
	int smallX = 0;
	int largeX = 0;
	int largestX = 0;
	int smallestY = 0;
	int smallY = 0;
	int largeY = 0;
	int largestY = 0;
	for (int i = 0; i < NUM_NOTCHES; i++){
		if (raw_cal_points_x[i*2] < raw_cal_points_x[smallestX]){// if it's the new smallest
			smallX = smallestX; // shuffle the old smallest to small
			smallestX = i*2; // record the new smallest index
		} else if (raw_cal_points_x[i*2] < raw_cal_points_x[smallX]){// if it's the new second-smallest
			smallX = i*2;// record the new small index
		}
		if (raw_cal_points_x[i*2] > raw_cal_points_x[largestX]){// if it's the new largest
			largeX = largestX;// shuffle the old largest to large
			largestX = i*2;// record the new largest index
		} else if (raw_cal_points_x[i*2] > raw_cal_points_x[largeX]){// if it's the new second-largest
			largeX = i*2;// record the new large index
		}
		if (raw_cal_points_y[i*2] < raw_cal_points_y[smallestY]){
			smallY = smallestY;
			smallestY = i*2;
		} else if (raw_cal_points_y[i*2] < raw_cal_points_y[smallY]){
			smallY = i*2;
		}
		if (raw_cal_points_y[i*2] > raw_cal_points_y[largestY]){
			largeY = largestY;
			largestY = i*2;
		} else if (raw_cal_points_y[i*2] > raw_cal_points_y[largeY]){
			largeY = i*2;
		}
	}
	// subtract the smallest and largest values
	cleaned_points_x[0] -= raw_cal_points_x[smallestX];
	cleaned_points_x[0] -= raw_cal_points_x[smallX];
	cleaned_points_x[0] -= raw_cal_points_x[largeX];
	cleaned_points_x[0] -= raw_cal_points_x[largestX];
	cleaned_points_y[0] -= raw_cal_points_y[smallestY];
	cleaned_points_y[0] -= raw_cal_points_y[smallY];
	cleaned_points_y[0] -= raw_cal_points_y[largeY];
	cleaned_points_y[0] -= raw_cal_points_y[largestY];

	//divide by the total number of calibration steps/2 to get the average origin value
	//except it's minus 4 steps since we removed outliers
	cleaned_points_x[0] = cleaned_points_x[0]/((float)NUM_NOTCHES-4);
	cleaned_points_y[0] = cleaned_points_y[0]/((float)NUM_NOTCHES-4);
}

void linearize_cal(const float cleaned_points_x[], const float cleaned_points_y[], float linearized_points_x[], float linearized_points_y[], stick_params_t *stick_params) {

	// for readability
	const float* in_x = cleaned_points_x;
	const float* in_y = cleaned_points_y;

	float* out_x = linearized_points_x;
	float* out_y = linearized_points_y;

	float fit_points_x[5];
	float fit_points_y[5];

	fit_points_x[0] = in_x[8+1];                     // right
	fit_points_x[1] = (in_x[6+1] + in_x[10+1])/2.0;  // right diagonal
	fit_points_x[2] = in_x[0];                       // center
	fit_points_x[3] = (in_x[2+1] + in_x[14+1])/2.0;  // left diagonal
	fit_points_x[4] = in_x[0+1];                     // left

	fit_points_y[0] = in_y[12+1];                    // down
	fit_points_y[1] = (in_y[10+1] + in_y[14+1])/2.0; // down diagonal
	fit_points_y[2] = in_y[0];                       // center
	fit_points_y[3] = (in_y[6+1] + in_y[2+1])/2.0;   // up diagonal
	fit_points_y[4] = in_y[4+1];                     // up	

	float* x_output = perfect_angles;
	float* y_output = perfect_angles;

	float temp_coeffs_x[FIT_ORDER + 1];
	float temp_coeffs_y[FIT_ORDER + 1];

	fitCurve(FIT_ORDER, 5, fit_points_x, x_output, FIT_ORDER+1, temp_coeffs_x);
	fitCurve(FIT_ORDER, 5, fit_points_y, x_output, FIT_ORDER+1, temp_coeffs_y);

	//write these coefficients to the array that was passed in, this is our first output
	for(int i = 0; i < (FIT_ORDER+1); i++){
		stick_params->fit_coeffs_x[i] = temp_coeffs_x[i];
		stick_params->fit_coeffs_x[i] = temp_coeffs_y[i];
	}

	for (int i = 0; i < NUM_NOTCHES; i++) {
		out_x[i] = linearize(in_x[i], stick_params->fit_coeffs_x);
		out_y[i] = linearize(in_y[i], stick_params->fit_coeffs_y);
	}
}