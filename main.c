#include<stdio.h>
#define MAX 1024
#define DEBUG 0

int mem[MAX];
char strings[MAX];
int lastmem,laststring;

#define iscons(x) (0<x && x<MAX)
#define isstr(x) (MAX<=x && x<2*MAX)
#define getstr(x) (strings+x-MAX)

const char initmem[]="nil\0t\0defun\0lambda\0cond\0atom\0eq\0car\0cdr\0cons\0quote\0print\0mem\0+\0-\0*\0/\0%";

void init(){
	lastmem=2; // so null pointers are valid
	for(int i=0;i<MAX;i++)mem[i]=strings[i]=0;
	laststring=sizeof(initmem);
	for(int i=0;i<laststring;i++)strings[i]=initmem[i];
}

#define car(x) mem[x]
#define cdr(x) mem[x+1]

/* not as useful as i thought it would be
int at(int a,int i){
	while(i--)a=cdr(a);
	return car(a);
}*/

int cons(int car,int cdr){
	mem[lastmem++]=car;
	mem[lastmem++]=cdr;
	return lastmem-2;
}

int intern(){
	// adapted from sectorlisp
	char c;
	for(int i=0;i<laststring;){ // iterates over strings in memory
		c=strings[i++];
		for(int j=0;;j++){
			if(!c&&!strings[laststring+j])return i-j-1+MAX; // reached end of new string, iow got a match
			if(c!=strings[laststring+j])break; // char dont match, stop testing
			c=strings[i++];
		}
		while(c)c=strings[i++]; // reels to beginning of next string
	}

	int ret=laststring; // cache beginning of new item
	while(strings[laststring++]); // update stack pointer
	return ret+MAX;
}

// ~~~ parsing

FILE*fd;char lookahead=0;

// XXX some of these can probably be defined
int iswhite(char a){return a==' '||a=='\n'||a=='\t';}
char getnext(){return lookahead=getc(fd);}
char white(){ // XXX this should prolly also skip over comments
	while(iswhite(lookahead))getnext();
	return lookahead;
}

int parselist(){
	white();
	if(lookahead==')' || lookahead==EOF){getnext();return 0;}

	int car;
	if(lookahead=='('){getnext();car=parselist();}
	else{ // newstring
		int i=0;
		while(!iswhite(lookahead) && lookahead!='(' && lookahead!=')'){
			strings[laststring+i++]=lookahead;
			getnext();
		}
		strings[laststring+i]=0;
		car=intern();
	}

	return cons(car,parselist());
}

// ~~~ utility prints

int pprint_(int i,int env){
	if(i==0){printf("<0>");return env;}
	if(i==env){printf("<etc %i>",i);return env;}
	if(i<env)env=i;

	if(i<MAX){
		putchar('(');
		pprint_(car(i),env);

		// pretty sure this can be improved
		// XXX make pprint return env?
		// XXX undo that, it doesnt do shit
		for(int t=cdr(i);t;t=cdr(t))
			if(t==env){printf(" <etc %i>)",t);return env;}
			else if(t<MAX)putchar(' '),env=pprint_(car(t),env);
			else putchar('.'),env=pprint_(t,env),t=0;

		putchar(')');
		return env;
	}

	//printf("%s",strings[i-MAX]);
	printf("%s",getstr(i));
	return env;
}

void pprint(int i,int env){pprint_(i,env);putchar(10);}

void printmem(int env){
	// XXX optimise this a bit?
	void pcell(int i){
		//if(isstr(i))printf("<%s>",strings[i-MAX]);
		if(isstr(i))printf("<%s>",getstr(i));
		else printf("%i",i);
	}

	printf("%i cons defined, %i atoms, env is %i\n",lastmem,laststring,env);

	for(int i=0;i<lastmem;i+=2){
		printf("%i:(",i);
		pcell(car(i));
		putchar('.');
		pcell(cdr(i));
		printf(") | ");
	}
	printf("\n\n");

	int i=0;
	while(strings[i] && i<laststring){
		printf("%i:",i);
		while(strings[i])putchar(strings[i++]);
		i++;
		printf(" | ");
	}
	printf("\n\n");
}

// ~~~ interpretation

// XXX ideally, use digit arithmetic
int atoi(int index){
	int res=0;
	char*str=strings+index-MAX;
	if(DEBUG)printf("atoi<%s>\n",str);
	for(;*str;str++){
		char a=*str;
		if(a<'0'||a>'9')
			printf("'%c' (%i) not a digit\n",a,a);
		else res=res*10+a-'0';
	}
	return res;
}

int itoa(int x){
	if(DEBUG)printf("itoa<%i>\n",x);
	if(x==0){
		strings[laststring]='0';
		strings[laststring+1]=0;
		return intern();
	}
	int ret=laststring,i=0;
	while(x){
		strings[laststring+i++]=(x%10)+'0';
		x/=10;
	}
	strings[laststring+i]=0;
	if(DEBUG)printf("int of length %i\n",i);
	char swap;
	for(int j=0;j<i/2;j++){
		swap=strings[laststring+j];
		strings[laststring+j]=strings[laststring+i-j-1];
		strings[laststring+i-j-1]=swap;
	}
	return intern();
}

int evlist(int exp,int env){
	// XXX rewrite this to be sequential and put it in eval
	if(!exp)return 0;
	return cons(eval(car(exp),env),evlist(cdr(exp),env));
}

int eval(int exp,int env){
	if(DEBUG){printf("> eval: ");pprint(exp,env);}
	if(!exp || exp==MAX)return 0;

	if(isstr(exp)){
		for(int cloj=env;cloj;cloj=cdr(cloj))
			for(int bind=car(cloj);bind;bind=cdr(bind))
				if(car(car(bind))==exp) // atoms are unique
					return cdr(car(bind));
		if(DEBUG)printf("> isself: "),pprint(exp,env);
		return exp;
	}

	// assuming cons
	int op = eval(car(exp),env);

	//if(DEBUG)printf("> op: "),pprint(op,env); // XXX this fucks up?

	if(isstr(op)){
		op-=MAX;

		// quote
		if(op==45)return car(cdr(exp));
		if(op==6){ // defun
			car(env) =
				cons(
					cons(
						car(cdr(exp)),
						eval(car(cdr(cdr(exp))),env)
					),car(env)
				);
			return car(cdr(exp));
		}

		if(op==12){ // lambda
			//exp = (lambda (<vars>) (<body>) . env)
			if(cdr(cdr(cdr(exp)))==0){
				if(DEBUG)printf("> set lenv %i\n",env);
				return cons(op,cons(car(cdr(exp)),cons(car(cdr(cdr(exp))), env )));
			}
			if(DEBUG)printf("> lenv:%i, env:%i\n",cdr(cdr(cdr(exp))),env);
			return exp;
		}

		if(op==19){ // cond
			for(int cond=cdr(exp);cond;cond=cdr(cond))
				if(eval(car(car(cond)),env)!=MAX)
					return eval(car(cdr(car(cond))),env);
			return 0;
		}

		// XXX could also map to another function? how do we take care of that?
		// eg eval(f)=subleq,eval(subleq)=(lambda ...)
		// try reevaluating it until it doesnt change?

		// apply
		int args=evlist(cdr(exp),env),res;

		if(DEBUG){printf("> apply: ");pprint(op,env);}

		switch(op){
			case 51: // print
				pprint(car(args),env);
				return car(args);
			case 57: // mem
				printmem(env);
				return 0;
			case 40: // cons
				return
				cons(
					car(args),
					car(cdr(args))
				);
			// car cdr atom eq - not sure abt returns
			case 32:return car(car(args));
			case 36:return cdr(car(args));
			case 24:return MAX+(isstr(car(args))?4:0);
			case 29:return MAX+(car(args)==car(cdr(args))?4:0);

			// XXX bignums
			case 61: // +
				res=0;
				for(;args;args=cdr(args))
					res+=atoi(car(args));
				return itoa(res);

			case 65: // *
				res=1;
				for(;args;args=cdr(args))
					res*=atoi(car(args));
				return itoa(res);

			case 63: // -
				res=atoi(car(args));
				for(args=cdr(args);args;args=cdr(args))
					res-=atoi(car(args));
				return itoa(res);

			case 67: // /
				res=atoi(car(args));
				for(args=cdr(args);args;args=cdr(args))
					res/=atoi(car(args));
				return itoa(res);

			case 69: // %
				res=atoi(car(args));
				for(args=cdr(args);args;args=cdr(args))
					res%=atoi(car(args));
				return itoa(res);

			default:
				printf("> not primfn:");pprint(op+MAX,env);
				return 0;
		}
	}

	if(car(op)==12){ // lambda
		if(DEBUG)printf("> application\n");
		// closure time!
		int cloj=0,names=car(cdr(op)),vals=cdr(exp);
		for(;names;names=cdr(names),vals=cdr(vals))
			cloj=cons(
				cons(
					car(names),
					eval(car(vals),env)
				),
				cloj);
		return eval(car(cdr(cdr(op))),cons(cloj,cdr(cdr(cdr(op)))));
	}

	printf("> wtf:");pprint(exp,env);

	return 0;
}

int main(int argc,char**argv){
	init();
	int env=cons(0,0),data=0;
	if(argc>1)fd=fopen(argv[1],"r");
	else fd=stdin;
	while(1){
		printf("> ");//fflush(0);
		if(feof(fd))break;
		// XXX i feel like it should be possible to do this somewhat better...
		getnext();white();getnext();
		data=parselist();
		if(DEBUG){printf("input: ");pprint(data,env);}
		data=eval(data,env);
		printf("< ");pprint(data,env);
		if(DEBUG)printf("\n\n\n");
	}
	putchar(10);
	fclose(fd);
	return 0;
}

// get rid of global lookahead (?)
//  or just generally package the parsing functions
// rename primfn to apply?

// try implementing streams and structures w/ internal state ala sicp
//  "structures w/ internal state" are gon have to be done thru closures and access functions
// streams gon need some peripherals for delayed evaluation, idk if those are gonna be as straight-forward
// do we wanna do electronic circuits as well?
// multithreading, arrays

// garbage collector still fucked up by the fact that defuns work outside and separately from evaluation
// does that make a difference? we keep stuff we defined (globally), everything else can go. if we define smth w/in a function, that definition is no longer valid nor needed once the function exits, so we can get rid of it
// and closures are tied to objects, so no issues there either

// imperative subsections - essentially a 'do' command, with somewhat different internals
//  let can take care of that - except we dont even use let...
// direct memory access - thats more a thing of 'put it in and see what happens'
//  gonna mess up the garbage collector even more, but eh
