Ok so I have a six-hour train ride ahead of me, a copy of the wizard book of programming, and a working gcc setup. Let's write a lisp interpreter.

## What is Lisp
Lisp, short for LISt Processing, is an old ('59, second after FORTRAN) programming language that had its own ideas on how to do things. It pioneered a lot of programming concepts, perhaps most famously practical recursion (which, while possible in ASM, isn't nearly as straight-forward or useful), functional programming (which is quite natural in a meta-language), garbage collecton (sparing us from having to manually manage memory like savages), repl loops (both foundational for modern languages like python) and object-oriented programming (which Smalltalk went on to perfect, and Java proceeded to fuck up). More importantly, it is a meta-programming language - iow most of the "programming" consists of modifying/expanding the language itself (well, technically it's mostly function definitions, but I digress). That makes all tasks roughly equal in difficulty, which is why it had been used extensively in AI programming and is nowadays popular for quantum computing, with minimal changes - essentially none to the language itself.

Instead of going with the imperative paradigm natural for hardware, Lisp was made for ease of evaluating and manipulating mathematical expressions. It wasn't really meant to work as a language, but with some clever ideas regarding command structure, it ended up being quite functional (pun intended). Let's take a look at those expressions. `f(x,y)=x^3+y^2` is a polynomial in the usual infix notation which is rather easy to read on paper but a nightmare for computers to parse (figuring out which operands are applied on which variables takes context). We could represent it in postfix notation as `xxx**yy*+` (using simplified befunge syntax - another fun language I recommend reading up on). That is trivially interpreted with a basic (imperative) stack automaton, as the function applications are themselves imperative, but it's impossible to work with from a mathematical perspective, as it is similarly hard to predict how the values we apply the operands on are constructed. Bracketed prefix notation on the other hand groups functions with their arguments as follows: `(+ (* x x x) (* y y))`. Now, each operand comes with its own context! It is trivial to tell which operands we apply in what order, and what their arguments are. Writing a function that takes derivatives, for example, can now be accomplished by first deriving all arguments (makes it easier to simplify expressions if we do that first), checking the current operand, and reshuffling the arguments accordingly (for an actual example, see "Structure and Interpretation of Computer Programs" Chapter 2.3.2). We can also take a hint from A. Church and wrap it in a lambda closure - `(lambda (x y) (+ (* x x x) (* y y)))` - giving us a context for the whole function (what variables it takes), and producing a valid lisp expression. This makes it quite easy to manipulate not only mathematical expressions, but also whole code structures. Indeed, a very useful aspect of Lisp is that it only needs a minimal set of instructions to be implemented, as the rest can be derived from those (some purists would argue that technically, you only need an implementation of `eval`, but that still includes other functions, just within a convenient wrapper). The set of instructions I'll be implementing is `defun lambda cond eq car cdr cons atom quote + - * / %` - a repl loop takes care of inputs and outputs, and we'll just bravely shove everything else aside.

## Lisp datatypes

Lisp essentially only has two datatypes. One of them is the `atom`, the individual strings that name all functions, variables, or simply signify themselves. Numbers are stored as a string of digits, which, while making it less efficient to work with them (as we cannot use the computer's existing hardware arithmetic functions for them, but have to do digit arithmetic), it means we also have arbitrary precision. Atoms we shall simply treat as strings. The other - as the name suggests - is lists, though those are handled in a somewhat unusual manner. As lists have arbitrary length and contents, they would be quite tricky to implement, yet Lisp has a trick up its sleeve and circumvents the issue by not implementing lists at all and instead implementing a binary structure known as concells. Each concell consists of two other objects, each of which can be an atom or another concell. Thus, if we denote a concell as `(a.b)` (commonly, `a` is referred to as the `car` and `b` as the `cdr`), the list `(a b c d e)` becomes `(a.(b.(c.(e.()))))`. The empty list `()` is a special case called `nil` and functions both as the `false` boolean and as a list terminator the same way a null byte terminates strings, and is equivalent to a nullpointer in C/C++ - try `printf("%p\n",0);`

Those we shall implement as two fixed-size arrays, for efficiency and 'simplicity' reasons. A C++ `std::vector` of a custom concell struct that denotes the types of its pointers with an enum would be the 'correct' way of implementing this, but I opted for a more basic, fixed-memory solution. Fun fact, Lisp Machines were mainframe-type computers optimized to run Lisp - if doing it in hardware was a good idea, then why overcomplicate our lives with 'best-practice' code?

```
## define MAX 4096
## define STRLEN 16
int mem[MAX*2]={0};
char strings[MAX*STRLEN]={0};
int lastmem=1,laststring=0;
```

`MAX` tells us how many objects we'll keep track of, `STRLEN` how long each atom is (remember what I said about fixed memory?). For convenience, we could have placed all the memory data in its own struct and passing that around - indeed, that is how we would have to do this if we were programming in Lisp itself -, but as we'll only be working on one process at a time, global memory is convenient. We have `MAX*2` elements in the concell memory, as each concell consists of two pointers (concell `i` has its car at `mem[2*i]` and cdr at `mem[2*i+1]`). To tell things apart, pointers less than `MAX` will point at concells, and pointers between `MAX` and `MAX*2` point at atoms. `lastmem` and `laststring` tell us how much of the memory buffers we've used - while Lisp may have pioneered garbage collection, we shan't burden ourselves with it yet, and simply suffer once we run out of memory. That being said, let's make some functions to make this more convenient.

```
## define iscons(x) (x<MAX)
## define isstr(x) (MAX<=x && x<2*MAX)
## define bool(x) (x?newstring_s("t"):0)

int car(int index){return mem[2*index];}
int cdr(int index){return mem[2*index+1];}
char*getstr(int index){return strings+(index-MAX)*STRLEN;}

int newcell(int car,int cdr){
	if(lastmem>=MAX)return 0; // hcf
	mem[2*lastmem]=car;
	mem[2*lastmem+1]=cdr;
	return lastmem++;
}

int newstring_s(char*string){
	for(int i=0;i<laststring;i++)
	if(strings[STRLEN*i])
		if(!strcmp(string,strings+STRLEN*i)) // lazy hack
			return i+MAX;

	if(laststring>=MAX)return 0; // hcf

	for(int i=0;i<STRLEN && string[i];i++)
		strings[laststring*STRLEN+i]=string[i];
	laststring++;
	return laststring-1+MAX;
}
```

`getstr` serves to give us a pointer to a string that we can use with eg `printf("%s\n",getstr(i));`. `newstring_s` conveniently only stores each atom once, returning a reference to its existing version if we come across it again.

And with that, we have all we need to represent Lisp programs and expressions. Lastly, let's define some functions for displaying all of that.

```
void pprint_(int i){
	if(i==0)printf("<0>");
	else if(i<MAX){
		putchar('(');
		pprint_(getcar(i));

		for(int t=getcdr(i);t;t=getcdr(t))
			if(t<MAX)putchar(' '),pprint_(getcar(t));
			else putchar('.'),pprint_(t),t=0;

		putchar(')');
	}else printf("%s",getstr(i));
}

void pprint(int i){pprint_(i);putchar(10);}

void printmem(){ // for debugging, prints the memory itself
	printf("%i cons defined, %i atoms\n",lastmem,laststring);
	for(int i=0;i<lastmem;i++)printf("%i:(%i.%i) | ",i,getcar(i),getcdr(i));putchar(10);
	for(int i=0;i<laststring;i++)printf("%i:<%s> | ",i+MAX,strings+i*STRLEN);putchar(10);
}
```

[XXX sanity check?]

## Parser

We can now represent Lisp expressions, and we'll soon figure out how to interpret them, but first, we'll have to be able to parse them, iow to take a program, consisting of text, and transform it into a tree of concells and atoms.

```
FILE*fd;char lookahead=0;

int iswhite(char a){return a==' '||a=='\n'||a=='\t';}
int isparen(char a){return a=='('||a==')'||a==EOF;}
char getnext(){return lookahead=getc(fd);}
char white(){
	while(iswhite(lookahead))getnext();
	return lookahead;
}
```

We'll need functions for reading atoms and concells. We could parse atoms by reading them into local memory and passing that to our previous function.

```
int newstring(){
	char buf[STRLEN]={0};
	for(int i=0;i<STRLEN && !iswhite(lookahead) && !isparen(lookahead);i++){
		buf[i]=lookahead;
		getnext();
	}

	return newstring_s(buf);
}
```

Parsing concells needs us to check whether we have a nil. Lists are a recursive structure made of concells terminated by nils, so that would actually be enough. Our other case check is whether the first element is a string or another concell, which fortunately simplifies down to whether it starts with a bracket.

```
int parselist(){
	white();
	if(lookahead==')' || lookahead==EOF){getnext();return 0;}

	int car;
	if(lookahead=='('){getnext();car=parselist();}
	else car=newstring();

	return newcell(car,parselist());
}

```

## Lisp functions

Before we get started on the interpreter (iow the part that takes all the code and structures and actually does stuff), let's actually go through what each of the primitive functions we'll be implementing does. For those of you who already have dipped their feet in Lisp - there isn't really a formal language specification as with eg python (ok that's a lie, [Common Lisp] very much exists, but it is by no means an authority), so this is also a definition of which functions I decided to include, and what syntax they use (`defun` specifically has a lot of variations across dialects).

`(quote a)` - this returns its argument unchanged. It might seem somewhat pointless to have a function that by definition doesn't do anything, however this ends up being quite relevant - functions in Lisp evaluate their arguments before applying, giving rise to eg the "first derive all arguments" described in the beginning. Without this, it would be impossible to specify pure data as arguments. The concept is further elaborated in the wizard book.

`(atom a)` - returns whether a is/refers to an atom (aka string) - an interface to `isstr`, so to speak. We can use `cons` to invert the output of this and thus test for concells. I elected to implement this because it's the one more frequently used to end recursion.

`(cons a b)` - makes a concell with `a` as the car and `b` as the cdr. This differs from `(quote (a b))` in that lists are nil-terminated, while `cons` allows us to set each element ourselves. This is also the foundation of custom datatypes.

`(car a)` and `(cdr a)` - return the car and cdr elements of `a`.

`(eq a b)` returns whether two elements are the same. More precisely, it evaluates both arguments, and recursively compares them - while we can simply compare atoms via their pointers, concells are recursive structures.

`(cond (<bool1> <expr1>) (<bool2> <expr2>) ...)` - conditionals! Also the main way of controlling program flow. `cond` iterates through its arguments and evaluates the car of each. If it evaluates to anything but false, the whole expression evaluates to the cdr of that subexpression. Better make sure it has a default at the end of the argument list! You can imagine this as a list of `else if`s.

`(lambda (<vars>) <expr>)` - `lambda` defines an anonymous function, as exemplified in the beginning. It takes a list of variables to bind, and provides an expression to evaluate, with the variables substituted. In practice, this works by creating a sub-environment, called a 'closure', wherein the variables are bound to their values. Some dialects (famously, Scheme) are implemented to the closure persists with the lambda, ie you can return a lambda from a function, use a lambda as an argument to another function, or [curry] a function, and the variables in the closure will persist (see [funarg problem]). I elected not to implement that, as it is a mess to implement and can relatively easily be implemented within Lisp if you really need it.

`(defun a b)` - going with the closure idea, this binds global variables. For the rest of the program, the atom `a` evaluates to the expression `b`. This allows us to define variables and functions (you could say it deanonymizes lambda functions).

`+-*/%` - arithmetics, work pretty much as you'd expect. The one exception to that is that they work on arbitrarily many arguments, which is a bit less straight-forward for the division and modulus.

As elaborated in the `quote` explanation, each function first evaluates its arguments, and then does its business with them. This bears repeating, as it has some implications for both implementation and command composition.

## Interpreter

Fortunately for the purists, the actual interpretng is a single `eval` function. Unfortunately, it is quite a long one, consisting of case checks for each form the expressions could take. We'll also ignore the bignum idea described above and convert numbers back and forth between digit and native representation for calculations (math is hard ok).

```
int atoi(int index){
	int res=0;
	for(int i=0;i<STRLEN;i++){
		char a=*(strings+(index-MAX)*STRLEN+i);
		if(a)res=res*10+a-'0';
	}
	return res;
}

int itoa(int i){
	char buf[STRLEN]={0};
	sprintf(buf,"%i",i);
	return newstring_s(buf);
}
```

Ok, that wasn't that hard. I was originally planning on storing all atoms backwards so I could easily implement carrying, but then decided to just leave all that for later. With numbers out of the way, let's make some wrappers for the complicated logic. For convenience, we'll separate the primitive functions (except for `defun`, as it is somewhat of a special case in that it modifies the global environment, and `lambda`, as it requires closures).

```
int eval(int exp,int env);

int primfn(int exp,int env){
	int fn=getcar(exp),args=getcdr(exp);

	if(fn==newstring_s("funcname")){
		// XXX INSERT CODE HERE
	}

	printf("not primfn:");pprint(fn);
	return 0;
}
```

Both take an expression to evaluate and the environment to evaluate it in. `primfn`s don't modify their environment, so they shall return what they evaluate to (as expected), yet `eval` might be a `define`, in which case we might need to also return the environment. It might be tempting to have the `env` argument be a pointer and use that for returns (I mean, tempting if you're a bit rattled in the noggin like me), yet the more 'natural' option is to return a concell whose car is the return value and its cdr is the new environment. It makes it somewhat cumbersome to retrieve values, but it makes sure we evaluate properly.

Now, to fill in the blanks.

```
if(fn==newstring_s("quote"))return args;
if(fn==newstring_s("lambda"))return exp;
```

Wait, what? Indeed, this checks out: `quote` returns its arguments unchanged (more importantly, unevaluated), and `lambda` simply returns itself, as it is a function that is applied, not evaluated (more on that in `eval`).

```
if(fn==newstring_s("atom"))return isstr(car(eval(car(args),env)))?newstring_s("t"):0;
if(fn==newstring_s("eq"))return car(eval(car(args),env)) == car(eval(car(cdr(args)),env))?newstring_s("t"):0;
```

The atom `t` is used to signify true. Instead of using some spaghetti gumbo-jumbo tag magic like all other interpreters insist on, we simply return the atom `t` and let the rest sort itself out. Technically, not even this is needed - since `cond` is the only function that cares about booleans, and since it treats `nil` as false and anything else as true, we could return anything that isn't `nil`.

```
if(fn==newstring_s("car"))return car(car(eval(car(args),env)));
if(fn==newstring_s("cdr"))return cdr(car(eval(car(args),env)));
if(fn==newstring_s("cons"))return cons(car(eval(car(args),env)),car(eval(car(cdr(args)),env)));
```

These are a bit more complicated. `args` is the list of arguments. `car(args)` is the first argument, ie the one we are applying the function(s) to (`car(cdr(args))` is the second etc). We evaluate the arguments (I told you this was gonna be relevant), then we take the `car` of that, because `eval` returns a `(value,env)` pair.

```
if(fn==newstring_s("cond")){
	for(int cond=args;cond;cond=cdr(cond))
		if(car(eval(car(car(cond)),env)))
			return car(eval(car(cdr(car(cond))),env));
	return 0;
}
```

This one should be pretty self-explanatory - it is a direct translation of the explanation above. The algebraics are all very identical - we make a variable to store the result in (potentially preloading it with the first argument), then iterate through the rest, applying them as we go.

```
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
```

And like that, we're done. This part was mostly an exercise in keeping track of how to index arguments, and not forgetting the signature of `eval`. Speaking of, let's get `eval`uating:

```
int eval(int exp,int env){
	if(!exp)return cons(0,env);
```

Well, duh - `nil` evaluates to `nil`, and it'd cause us a lot of issues if we tried parsing it with the rest.

```
	if(isstr(exp)){
		for(int val=env;val;val=cdr(val))
			if(car(car(val))==exp)
				return cons(cdr(car(val)),env);
		return cons(exp,env);
	}
```

If it's an atom, we go through the environment and check if we have a definition for its value. If not, it simply evaluates to itself. Yes, this makes it possible to redefine the values of `t` or the primitive functions - I elect to see this as a feature.

```
	if(isstr(car(exp))){
		int op=car(eval(car(exp),env));

		if(iscons(op))
			return eval(cons(op,cdr(exp)),env);

		if(op==newstring_s("defun")){
			int a = car(cdr(exp));
			int b = car(cdr(cdr(exp)));

			b = car(eval(b,env));

			int c = cons(a,b);
			env=cons(c,env);

			return cons(a,env);
		}
		return cons(primfn(cons(op,cdr(exp)),env),env);
	}
```

If we have an atom and then something else, that'd be a function application. First we check what that atom evaluates to. If it returns a compound expression, we are (most likely) dealing with a lambda - a recursive call to `eval` takes care of that [XXX]. If the atom evaluates to an(other) atom, it's most likely a primitive function. `defun` is handled separately, as explained above, the rest are delegated to a call to `primfn`. `defun` itself is also mostly a case of figuring out what argument goes where and how we index them. What `defun` actually evaluates to is arbitrary, as you're not supposed to use it as an argument. I elected to make it return the atom it binds, so you can use it as an argument anyway :).

```
	if(car(car(exp)) == newstring_s("lambda")){
		int cloj=env,names=car(cdr(car(exp))),vals=cdr(exp);
		for(;names;names=cdr(names),vals=cdr(vals))
			cloj=cons(cons(car(names),car(eval(car(vals),env))),cloj);
		int res = eval(car(cdr(cdr(car(exp)))),cloj);
		return cons(car(res),env);
	}
```

Ah, here's the fun one! For the closure, we take our current environment, and for each argument-variable pair, we append them to it, then evaluate the body of the lambda in that new environment. Easy!

If all else fails, we are most likely dealing with a compound expression, ie a list of expressions to be evaluated. In that case, we want to iterate through all of them, updating the environment as we go, and return the value of the last one (or rather, we return the current one if there is no next one - same statement, but easier to implement).

```
	int t = eval(car(exp),env);
	if(cdr(exp))return eval(cdr(exp),cdr(t));
	else return t;
}
```

And with that, we're done! Let's throw together a quick little repl and see it in action

```
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
```

We can either type stuff out by hand, or feed it files through `stdio`. I mostly used the second option . It also tends to smear up on multiple newlines when using it interactively, which is not something I feel like fixing rn xD

## Lisp programming

Now that we have a functional Lisp interpreter, let's write some code in Lisp. As Lisp is (in)famous for being a functional language and we have so far been working with an imperative language, let's implement some functional code. Most of the following operate on lists, by first doing something with the first element, and then recursively calling themselves on the rest of the list, if present. Thus, the general structure is the same.

```
(defun map (lambda (f x) (
	cond
	((eq x ()) ())
	(t (cons (f (car x)) (map f (cdr x))))
)))
```

Map goes through each element of the list and applies a function to it. If the list is empty, we're done, otherwise, apply the function to the first argument, map it to the rest of the list, and prepend one to the other.


Let's see it in (simplified) action:

```
(defun f (lambda (x) (* x x)))
(map f (quote 1 2 3 4 5))
> ((map f (1 2 3 4 5)))
> (1 (map f (2 3 4 5)))
> (1 4 (map f (3 4 5)))
> (1 4 9 (map f (4 5)))
> (1 4 9 16 (map f (5)))
> (1 4 9 16 25 (map f ()))
> (1 4 9 16 25)
```

Filter takes a list and a function and returns a new list which contains only the elements of the first list for which the function evaluates to true. This is accomplished by having a conditional as to whether we prepend the first element or not.

```
(defun filter (lambda (f x)(
	cond
	((eq x ()) ())
	(t (cond
		((f (car x)) (cons (car x) (filter f (cdr x))))
		(t (filter f (cdr x)))
	))
)))
```

Simplified action:

```
(defun f (lambda (x)(eq (% x 2) 0)))
(filter f (quote 1 2 3 4 5))
> ((filter f (1 2 3 4 5)))
> ((filter f (2 3 4 5)))
> (2 (filter f (3 4 5)))
> (2 (filter f (4 5)))
> (2 4 (filter f (5)))
> (2 4 (filter f ()))
> (2 4)
```

Range takes two numbers and returns a list with all numbers between them. Technically, ranges are a bit more complicated, but we'll do the easy version here. The keen reader might notice that this doesn't take a list as input, yet it returns one. This is in fact a bit of a problem, as we don't have a list to append/prepend our values to. This is where the `defun`s within `lambda`s come handy, as that allows us to make ourselves an auxilliary function, which we can then supply with an empty list.

```
(defun range (lambda (from to)(
	(defun range_acc (lambda (from to acc)
		(cond
			((eq from to) acc)
			(t (cons from (range_acc (+ from 1) to acc)))
		)
	))
	(range_acc from to ())
)))
```

Action:

```
(range 0 6)
> ((range 0 6))
> (0 (range 1 6))
> (0 1 (range 2 6))
> (0 1 2 (range 3 6))
> (0 1 2 3 (range 4 6))
> (0 1 2 3 4 (range 5 6))
> (0 1 2 3 4 5 (range 6 6))
> (0 1 2 3 4 5)
```

Folding functions do the metaphorical opposite of ranges, taking a bunch of numbers and a function (of two arguments), and returning a single number as a result.

```
(defun foldr (lambda (f x s) (
	cond
	((eq x ()) s)
	(t (f (car x) (foldr f (cdr x) s)))
)))

(defun foldl (lambda (f x s) (
	cond
	((eq x ()) s)
	(t (foldl f (cdr x) (f (car x) s)))
)))
```

Action!

```
(foldr + (quote 1 2 3 4 5) 0)
> (foldr + (1 2 3 4 5) 0)
> (+ 1 (foldr + (2 3 4 5) 0))
> (+ 1 (+ 2 (foldr + (3 4 5) 0)))
> (+ 1 (+ 2 (+ 3 (foldr + (4 5) 0))))
> (+ 1 (+ 2 (+ 3 (+ 4 (foldr + (5) 0)))))
> (+ 1 (+ 2 (+ 3 (+ 4 (+ 5 (foldr + () 0))))))
> (+ 1 (+ 2 (+ 3 (+ 4 (+ 5 0)))))
> (+ 1 (+ 2 (+ 3 (+ 4 5))))
> (+ 1 (+ 2 (+ 3 9)))
> (+ 1 (+ 2 12))
> (+ 1 14)
> 15

(foldl + (quote 1 2 3 4 5) 0)
> (foldl + (1 2 3 4 5) 0)
> (foldl + (2 3 4 5) 1)
> (foldl + (3 4 5) 3)
> (foldl + (4 5) 6)
> (foldl + (5) 10)
> (foldl + () 15)
> 15
```

Both arrive at the same result, but darn did they take different routes to get there! In fact, these two are quite common programming patterns, namely linearly recursive and tail-recursive. To illustrate them further, let's write a mandatory factorial program (fyi I am more or less going through the wizard book here, feel free to read up on this there):

```
(defun fac (lambda (x)(
	cond
	((eq x 0) 1)
	(t (* n (fac (- n 1))))
)))
(fac 5)
> (fac 5)
> (* 5 (fac 4))
> (* 5 (* 4 (fac 3)))
> (* 5 (* 4 (* 3 (fac 2))))
> (* 5 (* 4 (* 3 (* 2 (fac 1)))))
> (* 5 (* 4 (* 3 (* 2 (* 1 (fac 0))))))
> (* 5 (* 4 (* 3 (* 2 (* 1 1)))))
> (* 5 (* 4 (* 3 (* 2 1))))
> (* 5 (* 4 (* 3 2)))
> (* 5 (* 4 6))
> (* 5 24)
> 120
```

Now, let's look at a different implementation, one that uses an auxilliary function to implement tail recursion - ie a tail element to the function, an accumulator, where we store the current result:

```
(defun fact (lambda (x)
	(define fact_tail (x a)(
		cond
		((eq x 0) a)
		(t (fact_tail (- x 1) (* x a)))
	))
	(fact_tail x 1)
))
(fact 5)
> (fact_tail 5 1)
> (fact_tail 4 5)
> (fact_tail 3 20)
> (fact_tail 2 60)
> (fact_tail 1 120)
> (fact_tail 0 120)
> 120
```

This greatly saves on both the total number of computational steps, and on the stack frames (calls to `eval`) needed. Considering that we have limited memory (and don't want to wait too long), that is a great way of making things work.

To roll back a bit, why is functional programming such a big deal? Instead of giving the programmer a minimal set of complicated functions and requiring them to figure out how to get shit done, it gives them a ton of tiny functions and lets them daisy-chain the results together. In a way, it is a metaphor for what we have been doing - took a bunch of primitive functions, and assembled them in larger, more complex ones. [XXX got off the train. rewrite that last part - it sounds deranged. point out that low-func iterative is better suited for computers as it is how they do code at the most fundamental level]

Something something sum of all odd cubes of the numbers from 1 to 20:

```
res=0;
for(int i=0;i<20;i++){
	int t=i*i;
	if(t%2==0)
		res+=t;
}
```

```
(foldl +
	(filter
		(lambda (x) (eq (% x 2) 0))
		(map
			(lambda (x) (* x x))
			(range 0 21)
		)
	)
	0
)
```

[implement currying]
[compiler?]
[mention we dont use tags]
[elaborate on memory overflow & garbage collection]
[maybe elaborate on why exactly were doing this? as in how we get from math to lisp?]
