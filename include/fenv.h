#ifndef FENV_H_
#define FENV_H_

#define FE_INVALID		(1 << 0)
#define FE_DIVBYZERO	(1 << 2)
#define FE_OVERFLOW		(1 << 3)
#define FE_UNDERFLOW	(1 << 4)
#define FE_INEXACT		(1 << 5)
#define FE_ALL_EXCEPT	FE_DIVBYZERO | FE_INEXACT |	\
						FE_INVALID | FE_OVERFLOW |	\
						FE_UNDERFLOW

#define FE_DOWNWARD		0b01
#define FE_TONEAREST	0b00
#define FE_TOWARDZERO	0b11
#define FE_UPWARD		0b10

typedef struct {
	struct {
		unsigned short control_word;
		unsigned short __res1;
		unsigned short status_word;
		unsigned short __res2;
		unsigned short tag_word;
		unsigned short __res3;
		unsigned int instruction_pointer_offset;
		unsigned short instruction_pointer_selector;
		unsigned short opcode;
		unsigned int data_pointer_offset;
		unsigned short data_pointer_selector;
		unsigned short __res4;
	} __attribute__((packed)) fpu_env;
	unsigned int mxcsr;
} fenv_t;

typedef unsigned short fexcept_t;

extern int feclearexcept(int excepts);
extern int fetestexcept(int excepts);
extern int feraiseexcept(int excepts);
extern int fegetexceptflag(fexcept_t* flagp, int excepts);
extern int fesetexceptflag(const fexcept_t* flagp, int excepts);
int fesetround(int round);
int fegetround();
int fegetenv(fenv_t *envp);
int fesetenv(const fenv_t *envp);
int feholdexcept(fenv_t* envp);
int feupdateenv(const fenv_t *envp);

#endif /* FENV_H_ */