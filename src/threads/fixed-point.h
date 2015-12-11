#ifndef ___FIXED__POINT__PROTECTOR
#define ___FIXED__POINT__PROTECTOR

#define ___FIXED__POINT__MACROS
//#define ___FIXED__POINT__FUNC
#define FPARAM	(1<<14)

#ifdef ___FIXED__POINT__FUNC
typedef	int fpoint;

/*convert integer to fixed point*/
fpoint 
get_fixedpoint(int n)
{
	return FPARAM * n;
}

/*convert fixed point to integer (floor)*/
int 
get_int_floor(fpoint x)
{
	return x / FPARAM;
}

/*round up fixed point to integer*/
int 
get_int_round(fpoint x)
{
	if(x >= 0){
		return (x + FPARAM/2)/FPARAM;
	} else {
		return (x - FPARAM/2)/FPARAM;
	}
}
/*add two fixed point numbers*/
fpoint 
add(fpoint x, fpoint y){return x + y;}

/*subtracts two fixed point numbers*/
fpoint 
subtract(fpoint x, fpoint y){return x - y;}

/*multiply fixed points*/
fpoint 
multiply(fpoint x, fpoint y)
{
	return ((int64_t x)*y)/FPARAM;
}

/*divide fixed point nums*/
fpoint 
divide(fpoint x, fpoint y)
{
	return ((int64_t x)*FPARAM)/y;
}

/*mixed types operations*/
fpoint 
add_int(fpoint x , int n){return x + n*FPARAM;}

fpoint 
subtract_int(fpoint x , int n){return x - n*FPARAM;}

fpoint 
multiply_int(fpoint x , int n){return x*n;}

fpoint 
divide_int(fpoint x , int n){return x/n;}


#endif  /*normal functions mode*/

#ifdef ___FIXED__POINT__MACROS
#define fpoint int
/*conversion*/
#define TO_FP(x) (x) * (FPARAM)
#define TO_INT_FLOOR(x) ((x) / (FPARAM))
#define TO_INT_ROUND(x) (((x)<0)? ((x) + (FPARAM) / 2) / (FPARAM) : ((x) - (FPARAM) / 2) / (FPARAM))

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