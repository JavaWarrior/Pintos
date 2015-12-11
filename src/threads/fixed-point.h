#ifndef ___FIXED__POINT__PROTECTOR
#define ___FIXED__POINT__PROTECTOR

//#define ___FIXED__POINT__MACROS
//#define ___FIXED__POINT__FUNC
#define FPARAM	(1<<14)

#ifdef ___FIXED__POINT__FUNC
typedef	int fpoint;

/*conversion*/
fpoint get_fixedpoint(int n);
int get_int_floor(fpoint x);
int get_int_round(fpoint x);

/*uni type operations*/
fpoint add(fpoint x, fpoint y);
fpoint subtract(fpoint x, fpoint y);
fpoint multiply(fpoint x, fpoint y);
fpoint divide(fpoint x, fpoint y);

/*mixed types operations*/
fpoint add_int(fpoint x , int n);
fpoint subtract_int(fpoint x , int n);
fpoint multiply_int(fpoint x , int n);
fpoint divide_int(fpoint x , int n);


#endif  /*normal functions mode*/

#ifdef ___FIXED__POINT__MACROS
#define fpoint int
/*conversion*/
#define CONVERT_TO_FP(x) (x) * (FPARAM)
#define CONVERT_TO_INT_ZERO(x) ((x) / (FPARAM))
#define CONVERT_TO_INT_NEAR(x) (((x)<0)? ((x) + (FPARAM) / 2) / (FPARAM) : ((x) - (FPARAM) / 2) / (FPARAM))

/*uni type operations*/
#define ADD_FP(x, y) ((x) + (y))
#define SUB_FP(x, y) ((x) - (y))
#define MUL_FP(x, y) (((int64_t)(x)) * (y)) / (FPARAM)
#define DIV_FP(x, y) (((int64_t)(x)) * (FPARAM)) / (y)

/*mixed types operations*/
#define ADD_INT(x, n) ((x) + (n) * (FPARAM))
#define SUB_INT(x, n) ((x) - (n) * (FPARAM))
#define MUL_INT(x, n) ((x) * (n))
#define DIV_INT(x, n) ((x) / (n))
#endif /*macros mode*/
#endif