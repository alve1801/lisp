#include<stdio.h>
#define MAX 5000 // 4096, but base 10 makes it easier to read in debugger
#define STRLEN 16
#define DEBUG 0

// the memory arrays in question. mem stores concells, strings stores atoms
// the ith concell has its car at position 2*i and its cdr at pos 2*i+1
// string indices are array index + MAX
// keep in mind that first element of mem is reserved for null
int mem[MAX*2]={0};
char strings[MAX*STRLEN]={0};

// the current top of stack
int lastmem=1,laststring=0;

void init(){
	// apparently weve been having issues w/ this?
	for(int i=0;i<MAX*2;i++)mem[i]=0;
	for(int i=0;i<MAX*STRLEN;i++)strings[i]=0;
}

int car(int index){return mem[2*index];}
int cdr(int index){return mem[2*index+1];}
char*getstr(int index){return strings+(index-MAX)*STRLEN;}

int cons(int car,int cdr){
	// garbage collector?
	if(lastmem>=MAX){
		printf("memory overflow!\n");
		return 0; // XXX hcf
	}
	mem[2*lastmem]=car;
	mem[2*lastmem+1]=cdr;
	return lastmem++;
}

int newstring_s(char*string){
	// this assumes string memory management is handled externally
	for(int i=0;i<laststring;i++)
	if(strings[STRLEN*i]){
		/* XXX still fucked
		int same=1;
		for(int j=0;j<STRLEN && strings[STRLEN*i+j];j++)
			if(string[j]!=strings[STRLEN*i+j])
				same=0;
		if(same)return i+MAX;
		*/
		if(!strcmp(string,strings+STRLEN*i))return i+MAX;
	}

	if(laststring>=MAX){
		printf("strings overflow!\n");
		return 0; // XXX hcf
	}

	for(int i=0;i<STRLEN && string[i];i++){
		strings[laststring*STRLEN+i]=string[i];
	}
	laststring++;
	return laststring-1+MAX;
}

#define iscons(x) (x<MAX)
#define isstr(x) (MAX<=x && x<2*MAX)

// ~~~ utility prints

void pprint_(int i){
	if(i==0)printf("<0>");
	else if(i<MAX){
		putchar('(');
		pprint_(car(i));

		for(int t=cdr(i);t;t=cdr(t))
			if(t<MAX)putchar(' '),pprint_(car(t));
			else putchar('.'),pprint_(t),t=0;

		putchar(')');
	}else printf("%s",getstr(i));
}

void pprint(int i){pprint_(i);putchar(10);}

void printmem(){
	// for debugging, prints the memory itself
	printf("%i cons defined, %i atoms\n",lastmem,laststring);
	for(int i=0;i<lastmem;i++)
		printf("%i:(%i.%i) | ",i,car(i),cdr(i));
	putchar(10);
	for(int i=0;i<laststring;i++)
		printf("%i:<%s> | ",i+MAX,strings+i*STRLEN);
	putchar(10);
}

// ~~~ parsing

FILE*fd;char lookahead=0;

int iswhite(char a){return a==' '||a=='\n'||a=='\t';}
int isparen(char a){return a=='('||a==')'||a==EOF;}
char getnext(){return lookahead=getc(fd);}
char white(){
	while(iswhite(lookahead))getnext();
	return lookahead;
}

int newstring(){
	// read from stdio and process that
	char buf[STRLEN]={0};
	for(int i=0;i<STRLEN && !iswhite(lookahead) && !isparen(lookahead);i++){
		buf[i]=lookahead;
		getnext();
	}

	return newstring_s(buf);
}

int parselist(){
	white();
	if(lookahead==')' || lookahead==EOF){getnext();return 0;}

	int car;
	if(lookahead=='('){getnext();car=parselist();}
	else car=newstring();

	return cons(car,parselist());
}

// ~~~ interpretation

int eval(int exp,int env);

int atoi(int index){
	int res=0;
	for(int i=0;i<STRLEN;i++){
		char a=*(strings+(index-MAX)*STRLEN+i);
		if(!a)break;
		if(a<'0'||a>'9')printf("'%c' (%i) not a digit\n",a,a);
		else res=res*10+a-'0';
	}
	return res;
}

int itoa(int i){
	char buf[STRLEN]={0};
	sprintf(buf,"%i",i);
	return newstring_s(buf);
}

int primfn(int exp,int env){
	int fn=car(exp),args=cdr(exp);
	if(DEBUG){printf("prime func: ");pprint(fn);}

	// helpers
	if(fn==newstring_s("mem")){
		printmem();
		printf("env:%i:",env);pprint(env);
		return 0;
	}

	if(fn==newstring_s("display")){
		int t=car(eval(car(args),env));
		pprint(t);
		return t;
	}

	// arguments are evaluated in the wrong order - not sure if that makes a difference
	if(fn==newstring_s("cons")){
		return
		cons(
			car(eval(car(args),env)),
			car(eval(car(cdr(args)),env))
		);
	}

	if(fn==newstring_s("car"))return car(car(eval(car(args),env)));
	if(fn==newstring_s("cdr"))return cdr(car(eval(car(args),env)));
	if(fn==newstring_s("atom"))return isstr(car(eval(car(args),env)))?newstring_s("t"):0;
	if(fn==newstring_s("eq"))return car(eval(car(args),env)) == car(eval(car(cdr(args)),env))?newstring_s("t"):0;
	if(fn==newstring_s("quote"))return args;
	if(fn==newstring_s("lambda"))return exp;

	if(fn==newstring_s("cond")){
		for(int cond=args;cond;cond=cdr(cond))
			if(car(eval(car(car(cond)),env)))
				return car(eval(car(cdr(car(cond))),env));
		return 0;
	}

	// XXX bignums
	if(fn==newstring_s("+")){
		int res=0;
		for(;args;args=cdr(args))
			res+=atoi(car(eval(car(args),env)));
		return itoa(res);
	}

	if(fn==newstring_s("*")){
		int res=1;
		for(;args;args=cdr(args))
			res*=atoi(car(eval(car(args),env)));
		return itoa(res);
	}

	if(fn==newstring_s("-")){
		int res=atoi(car(eval(car(args),env)));
		for(args=cdr(args);args;args=cdr(args))
			res-=atoi(car(eval(car(args),env)));
		return itoa(res);
	}

	if(fn==newstring_s("/")){
		int res=atoi(car(eval(car(args),env)));
		for(args=cdr(args);args;args=cdr(args))
			res/=atoi(car(eval(car(args),env)));
		return itoa(res);
	}

	if(fn==newstring_s("%")){
		int res=atoi(car(eval(car(args),env)));
		for(args=cdr(args);args;args=cdr(args))
			res%=atoi(car(eval(car(args),env)));
		return itoa(res);
	}

	printf(">not primfn:");pprint(fn);
	return 0;
}

int eval(int exp,int env){
	if(DEBUG){printf("> ");pprint(exp);}
	if(!exp)return cons(0,env);

	if(isstr(exp)){
		for(int val=env;val;val=cdr(val))
			if(car(car(val))==exp) // since both are already in-memory
				return cons(cdr(car(val)),env);
		return cons(exp,env);
	}

	// XXX evaluate all elements of exp then match first?

	if(isstr(car(exp))){
		int op=car(eval(car(exp),env));

		if(DEBUG){printf(">newfun:");pprint(op);}

		if(iscons(op)) // function application, aka lambda
			// XXX consider making this a fallthrough instead?
			return eval(cons(op,cdr(exp)),env);

		// only predefined function that changes the environment
		if(op==newstring_s("defun")){
			// XXX this can prolly be simplified...
			int a = car(cdr(exp));
			int b = car(cdr(cdr(exp)));

			b = car(eval(b,env));

			int c = cons(a,b);
			env=cons(c,env);

			return cons(a,env);
		}

		//return cons(primfn(exp,env),env);
		return cons(primfn(cons(op,cdr(exp)),env),env);
	}

	// XXX check this exists before referencing
	// eg car(exp) could be null
	// maybe put t here?
	if(car(car(exp)) == newstring_s("lambda")){
		// closure time!
		int cloj=env,names=car(cdr(car(exp))),vals=cdr(exp);
		for(;names;names=cdr(names),vals=cdr(vals))
			cloj=cons(cons(car(names),car(eval(car(vals),env))),cloj);
		int res = eval(car(cdr(cdr(car(exp)))),cloj);
		return cons(car(res),env);
	}

	// XXX if all else fails, try evaluating arguments in order?
	//return eval(cdr(exp),cdr(eval(car(exp))));

	int t = eval(car(exp),env);
	if(cdr(exp))return eval(cdr(exp),cdr(t));
	else return t;

	printf(">wtf:");pprint(exp);

	return 0;
}

int main(){
	init();
	int env=0;
	fd=stdin;
	while(1){
		if(feof(fd))return 0;
		getnext();
		white();
		getnext();
		white();
		int data=parselist();
		//pprint(data);
		int res=eval(data,env);
		env=cdr(res);
		pprint(car(res));
	}
	return 0;
}

// rename car cdr to just car cdr
// newcell iscoll etc are shitty names, consolidate
// get rid of global lookahead

// cond map let/defun eval apply quote lambda +-*/=
// defun lambda cond eq car cdr cons quote +-*/%
// atom?

// XXX well need to have something that goes thru a list and evaluates as it goes
// (define f lambda (x y) (define sq lambda (x) (* x x)) (+ (sq x) (sq y)))
// i mean, LET exists, tho idk list of instructions makes more sense imo...
// we could also treat DEFUNs as a function applying a closure to the rest of the code block
// sounds cool, but its a specific solutoin, wed like a general one. any list of expressions evaluates to the last one (unless context implies otherwise)
// we could have a wrapper for that, but what would the usecase be?
//  a=eval(car(exp));if(!cdr(exp))return a; else evlist(b)
// cant we just do this at the bottom of eval? if all else fails, evaluate car, then iterate over cdr?

// can we pass env as a pointer to eval? and then just have it set that?
// also its ONE FUCKING COMMAND that actually does shit to env, wtf

// newlines cause the next expr to be wrapped in a cell

// closures r fucked, curryin dont work
// closures are a useful, but not NECESSARY, part of lisp, so why bother

// (let (a) (b) c) - binds values in b to atoms in a, then evaluates c in the new closure
// we might have to get a bit obsessive about keeping environments throughout operations
// either that, or we do the pass-by-reference thing
// (do a) - foreach in a, evaluate it, return last
// (lambda args body . env) - store closure in lambda
// how will we combine that env with the current one tho? only way i can think is that eval must take a stack of environments, which kinda fucks shit up...
// also, where exactly do we store the closure? what even is the closure exactly?

// garbage collector
// shouldnt even be that hard, might wanna include a tag on whether a cell is free tho
// (()) = (().()) = 0,0 in memory, so we cant check for that. id argue its a stupid expression, but still.
//  wait, since all of those would be identical, we could repoint all instances of that to nil - wait fuck. nvm.
// welp, tags it is! 1<<(sizeof(int)-1) should do
