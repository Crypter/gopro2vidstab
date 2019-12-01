#ifndef anglesmath_hpp
#define anglesmath_hpp

#include <cmath>

class pixel_travel_t{
public:
        double x=0;
        double y=0;
        double r=0;
        double t=0;

        void set_cartesian(double x, double y){
			this->x = x;
			this->y = y;

			this->r = sqrt(x*x + y*y);
			this->t = atan2(x,y);
        }

        void set_polar(double r, double t){
			this->r = r;
			this->t = t;

			while (this->t > M_PI){
					this->t -= M_2_PI;
			}
			while (this->t < -M_PI){
					this->t += M_2_PI;
			}

			this->x = this->r * sin(this->t);
			this->y = this->r * cos(this->t);

        }

        pixel_travel_t operator - (const pixel_travel_t &object){
                pixel_travel_t result;
                result.set_cartesian(this->x - object.x, this->y - object.y);
                return result;
        }

        pixel_travel_t operator + (const pixel_travel_t &object){
                pixel_travel_t result;
                result.set_cartesian(this->x + object.x, this->y + object.y);
                return result;
        }

        pixel_travel_t operator = (const pixel_travel_t &object){
                this->x = object.x;
                this->y = object.y;
                this->r = object.r;
                this->t = object.t;
                return *this;
        }
};

class angles_t{
public:
	double x = 0;
	double y = 0;
	double z = 0;
};


class frame_modifier_t{
public:
	pixel_travel_t original;
	pixel_travel_t adjust;
	void reset(){
		adjust = original;
	};
};

#endif
