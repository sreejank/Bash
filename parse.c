//Parses Bash command into a parse to tree for process to execute. 

#define _GNU_SOURCE
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>


struct elt2 { 
	struct elt2 *next;
	CMD *value;
};

typedef struct elt2 *Stack2;

#define STACK_EMPTY (0)

void
stackPush2(Stack2 *s, CMD *value) {
    struct elt2 *e;
    e = malloc(sizeof(struct elt2));
    assert(e);
    e->value = value;
    e->next = *s;
    *s = e;
}

bool stackEmpty2(const Stack2 *s) {
    return (*s == 0);
}

CMD *stackPop2(Stack2 *s) {
	CMD * ret;
    struct elt2 *e;

    assert(!stackEmpty2(s));

    ret = (*s)->value;

    /* patch out first element */
    e = *s;
    *s = e->next;

    free(e);

    return ret;
}
void stackPrint2(const Stack2 *s) {
    struct elt2 *e;

    for(e = *s; e != 0; e = e->next) {
    	if(e->value->type==0) {
    		printf("%s\n",e->value->argv[0]);
    	}
    	else {
    		printf("%d\n",e->value->type);
    	}
    }
    
    putchar('\n');
}



char *myconcat(const char *s1, const char *s2) {
	char *result=malloc(strlen(s1)+strlen(s2)+1);
	strcpy(result,s1);
	strcat(result,s2);
	return result;
}

//Precedence:
//Redirections (<, >, etc.)
//Pipes (|)
//Short-circuiting booleans (&&, ||)
//Terminators (&, ;)


//Pattern match for var=value
bool isLocalVariable(char *text) {
	int index=0;
	if(text[0]=='=') {
		return false;
	}
	bool hasEqual=false;
	while(index<strlen(text)) {
		if(text[index]=='=') {
			hasEqual=true;
			break;
		}
		if(index==0 && isdigit(text[index])) {
			return false;
		}
		if(!isalnum(text[index]) && text[index]!='_') {
			return false;
		}
		index++;
	}
	if(!hasEqual) {
		return false;
	}
	return true;
}

bool hasGreaterPrecedence(int o1, int o2) {
	int v1=-1;
	int v2=-1;
	if(o1==PIPE) {
		v1=2;
	}
	if(o2==PIPE) {
		v2=2;
	}
	if(o1==SEP_AND || o1==SEP_OR) {
		v1=1;
	}
	if(o2==SEP_AND || o2==SEP_OR) {
		v2=1;
	}
	if(o1==SEP_END || o1==SEP_BG) {
		v1=0;
	}
	if(o2==SEP_END || o2==SEP_BG) {
		v2=0;
	}
	return v1>=v2;

}

token *tok;
//Add 1 for left, subtract 1 for right
int numParen=0;


//Main Parse Command
CMD *parse_aux(token *list) {
	if(list==NULL) {
		return NULL;
	}
	tok=list;
	//CMD *cmd;

	Stack2 operatorStack=STACK_EMPTY;

	Stack2 cmd_stack=STACK_EMPTY;


	bool redirectedIn=false;
	bool redirectedOut=false;
	bool redirectedErr=false;


	while(tok!=NULL) {
		//**Make Simple Token**
		if(RED_OP(tok->type) || tok->type==SIMPLE || tok->type==PAR_LEFT) {
			redirectedIn=false;
			redirectedOut=false;
			redirectedErr=false;
			CMD *simple_cmd=mallocCMD();

			
			simple_cmd->argc=0;
			int argIndex=-1;

			simple_cmd->locVar=calloc(sizeof(char *),_POSIX_ARG_MAX);
			simple_cmd->locVal=calloc(sizeof(char *),_POSIX_ARG_MAX);

			//char **newbuff;
			free(simple_cmd->argv);
			simple_cmd->argv=calloc(sizeof(char *),_POSIX_ARG_MAX);

			
			while(tok!=NULL && (RED_OP(tok->type) || tok->type==SIMPLE || tok->type==PAR_LEFT)) {
				//printf("I have token %s with type %d\n",tok->text,tok->type);
				if(RED_OP(tok->type)) {
					if(tok->type==RED_IN) {
						if(redirectedIn) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: Two input redirects\n");
							return NULL;
						}
						redirectedIn=true;
						simple_cmd->fromType=tok->type;
						tok=tok->next;
						if(tok==NULL || tok->type!=SIMPLE) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: error, no file after redirection\n");
							return NULL;
						}
						simple_cmd->fromFile=strdup(tok->text);
					}
					
					else if(tok->type==RED_IN_HERE) {
						if(redirectedIn) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: Two input redirects\n");
							return NULL;
						}
						redirectedIn=true;
						simple_cmd->fromType=tok->type;

						tok=tok->next;
						if(tok==NULL || tok->type!=SIMPLE) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: error, no file after redirection\n");
							return NULL;
						}


						char *end_text=myconcat(tok->text,"\n");

						char *line=NULL;
						char *document=strdup("");
						size_t nLine=0;
						while(getline(&line,&nLine,stdin)>0) {
							if(strcmp(line,end_text)==0) {
								break;
							}
							else {
								char *old_document=document;
								document=myconcat(document,line);
								free(old_document);
							}
						}
						free(line);
						free(end_text);

						long new_doc_size=0;
						int index=0;

						//int change_points[strlen(document)][2];
						//for(int x=0;x<strlen(document);x++) {
						//	change_points[x][0]=-1;
						//	change_points[x][1]=-1;
						//}
						//int cp_index=0;

						while(index<strlen(document)) {
							if(document[index]=='$') {
								//change_points[cp_index][0]=index;
								char *var_name=malloc(sizeof(document));
								int var_index=0;
								index++;
								while(index<strlen(document) && !isspace(document[index])) {
									var_name[var_index++]=document[index];
									index++;
								}

								var_name[var_index]='\0';
								char *var_value=getenv(var_name);
								if(var_value==0) {
									var_value="";
								}
								new_doc_size+=strlen(var_value)+1;
								free(var_name);

								//change_points[cp_index++][1]=index;
								index++;
							}
							else if(document[index]=='\\' && index+1<strlen(document) && (document[index+1]=='\\' || document[index+1]=='$')) {
								new_doc_size++;
								index+=2;
							}
							else {
								new_doc_size++;
								index++;
							}
						}
						//printf("Size of new here doc:  %lu, old doc: %ld\n",new_doc_size,strlen(document));
						index=0;
						int index_new=0;
						char *processed_doc=calloc(1,new_doc_size+1);


						while(index<strlen(document)) {
							if(document[index]=='$') {
								char *var_name=malloc(sizeof(document));
								int var_index=0;
								index++;
								while(index<strlen(document) && !isspace(document[index])) {
									var_name[var_index++]=document[index];
									index++;
								}

								var_name[var_index]='\0';
								char *var_value=getenv(var_name);
								if(var_value==0) {
									var_value="";
								}
								for(int var_name_index=0;var_name_index<strlen(var_value);var_name_index++) {
									processed_doc[index_new++]=var_value[var_name_index];
								}
								free(var_name);
							}
							else if(document[index]=='\\' && index+1<strlen(document) && (document[index+1]=='\\' || document[index+1]=='$')) {
								index++;
								processed_doc[index_new++]=document[index];
								index++;
							}
							else {
								processed_doc[index_new++]=document[index];
								index++;
							}
						}
						if(new_doc_size>0 && processed_doc[new_doc_size-1]!='\n') {
							char *old_doc=processed_doc;
							processed_doc=myconcat(processed_doc,"\n");
							free(old_doc);
						}
						free(document);
						simple_cmd->fromFile=processed_doc;
					}
					else if(tok->type==RED_OUT || tok->type==RED_OUT_APP || tok->type==RED_OUT_ERR ) {
						if(redirectedOut) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: Two output redirects\n");
							return NULL;
						}
						redirectedOut=true;
						simple_cmd->toType=tok->type;
						tok=tok->next;
						if(tok==NULL || tok->type!=SIMPLE) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: error, no file after redirection\n");
							return NULL;
						}
						simple_cmd->toFile=strdup(tok->text);
					}
					else if(tok->type==RED_ERR || tok->type==RED_ERR_APP) {
						if(redirectedErr) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: Two error redirects\n");
							return NULL;
						}
						redirectedErr=true;
						simple_cmd->errType=tok->type;
						tok=tok->next;
						if(tok==NULL || tok->type!=SIMPLE) {
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: error, no file after redirection\n");
							return NULL;
						}
						simple_cmd->errFile=strdup(tok->text);
					}
					else {
						freeCMD(simple_cmd);
						fprintf(stderr,"How did we get here?\n");
						return NULL;
					}
				}
				else if(tok->type==PAR_LEFT) {
					if(simple_cmd->type==SIMPLE) {
						freeCMD(simple_cmd);
						fprintf(stderr,"Parse: command and subcommand\n");
						return NULL;
					}
					simple_cmd->type=SUBCMD;
					numParen++;
					if(tok->next==NULL) {
						freeCMD(simple_cmd);
						fprintf(stderr,"Parse: Imbalanced parenthesis\n");
						return NULL;
					}
					CMD *subcmd=parse_aux(tok->next);
					if(subcmd==NULL) {
						freeCMD(simple_cmd);
						while(!stackEmpty2(&cmd_stack)) {
							freeCMD(stackPop2(&cmd_stack));
						}
						while(!stackEmpty2(&operatorStack)) {
							freeCMD(stackPop2(&operatorStack));
						}
						return NULL;
					}
					else {
						simple_cmd->left=subcmd;
					}
				}
				else {
					//Local variable
					if(isLocalVariable(tok->text) && argIndex<0) {
						char *varname=malloc(strlen(tok->text));
						char *varValue=NULL;
						int index=0;
						while(index<strlen(tok->text) && tok->text[index]!='=') {
							varname[index]=tok->text[index];
							index++;
						}
						varname[index]='\0';
						index++;
						//printf("%d\n",index);
						if(index<strlen(tok->text)) {
							int index2=0;
							varValue=malloc(strlen(tok->text));
							while(index<strlen(tok->text)) {
								varValue[index2]=tok->text[index];
								index++;
								index2++;
							} 
							varValue[index2]='\0';
						}
						if(varValue!=NULL) {
							//bool foundVar=false;
							/*
							for(int i=0;i<simple_cmd->nLocal;i++) {
								if(strcmp(varname,simple_cmd->locVar[i])==0) {
									free(simple_cmd->locVal[i]);
									free(varname);
									simple_cmd->locVal[i]=varValue;
									foundVar=true;
								}
							}
							*/
							//if !foundVar
							if(simple_cmd->nLocal+1>_POSIX_ARG_MAX) {
								freeCMD(simple_cmd);
								fprintf(stderr,"Parse: Too many local variables\n");
								return NULL;
							}
							simple_cmd->locVar[simple_cmd->nLocal]=varname;
							simple_cmd->locVal[simple_cmd->nLocal]=varValue;
							simple_cmd->nLocal++;
						}
						else {
							simple_cmd->locVar[simple_cmd->nLocal]=varname;
							simple_cmd->locVal[simple_cmd->nLocal]=strdup("");
							simple_cmd->nLocal++;
						}

					}
					else {
						//printf("I am at token %s\n",tok->text);
						if(simple_cmd->type==SUBCMD) {
							while(!stackEmpty2(&cmd_stack)) {
								freeCMD(stackPop2(&cmd_stack));
							}
							while(!stackEmpty2(&operatorStack)) {
								freeCMD(stackPop2(&operatorStack));
							}
							freeCMD(simple_cmd);
							fprintf(stderr,"Parse: command and subcommand\n");
							return NULL;
						}
						else {
							simple_cmd->type=SIMPLE;
						}
						if(simple_cmd->argc+1>_POSIX_ARG_MAX) {
							while(!stackEmpty2(&cmd_stack)) {
								freeCMD(stackPop2(&cmd_stack));
							}
							while(!stackEmpty2(&operatorStack)) {
								freeCMD(stackPop2(&operatorStack));
							}
							fprintf(stderr,"Parse: too many arguments\n");
							freeCMD(simple_cmd);
							return NULL;
						}
						argIndex++;
						simple_cmd->argv[argIndex]=strdup(tok->text);
						(simple_cmd->argc)++;
					}
				}
				tok=tok->next;
			}
			if(simple_cmd->nLocal==0) {
				free(simple_cmd->locVar);
				simple_cmd->locVar=NULL;
				free(simple_cmd->locVal);
				simple_cmd->locVal=NULL;
			}
			if(simple_cmd->argc==0 && (simple_cmd->type==SIMPLE || simple_cmd->type==NONE)) {
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				freeCMD(simple_cmd);
				fprintf(stderr,"Parse: null command\n");
				return NULL;
			}
			stackPush2(&cmd_stack,simple_cmd);
		}
		//**End Make Simple Token**
		//**Case for Right Paren
		else if(tok->type==PAR_RIGHT) {
			numParen--;
			if(numParen<0) {
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: mismatched parenthesis\n");
				return NULL;
			}
			//tok=tok->next;
			while(!stackEmpty2(&operatorStack)) {
				CMD *operator=stackPop2(&operatorStack);
				if(stackEmpty2(&cmd_stack)) {
					freeCMD(operator);
					while(!stackEmpty2(&operatorStack)) {
						freeCMD(stackPop2(&operatorStack));
					}
					fprintf(stderr,"Parse: Not enough operands for operator\n");
					return NULL;

				}
				CMD *operand2=stackPop2(&cmd_stack);
				if(stackEmpty2(&cmd_stack) && (operator->type==PIPE || operator->type==SEP_AND || operator->type==SEP_OR)) {
					freeCMD(operator);
					freeCMD(operand2);
					while(!stackEmpty2(&cmd_stack)) {
						freeCMD(stackPop2(&cmd_stack));
					}
					while(!stackEmpty2(&operatorStack)) {
						freeCMD(stackPop2(&operatorStack));
					}
					fprintf(stderr,"Parse: Not enough operands for operator\n");
					return NULL;
				}
				else if(stackEmpty2(&cmd_stack)) {
					operator->left=operand2;
					stackPush2(&cmd_stack,operator);
				}
				else {
					CMD *operand1=stackPop2(&cmd_stack);
					if(operator->type==PIPE && operand2->fromFile!=NULL) {
						freeCMD(operand1);
						freeCMD(operand2);
						freeCMD(operator);
						while(!stackEmpty2(&cmd_stack)) {
							freeCMD(stackPop2(&cmd_stack));
						}
						while(!stackEmpty2(&operatorStack)) {
							freeCMD(stackPop2(&operatorStack));
						}
						fprintf(stderr,"Parse: Two input redirects\n");
						return NULL;

					}
					operator->left=operand1;
					operator->right=operand2;
					stackPush2(&cmd_stack,operator);
				}
			}
			if(stackEmpty2(&cmd_stack)) {
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: Null command.\n");
				return NULL;
			}
			if(cmd_stack->next!=0) {
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: Not enough operators.\n");
				return NULL;
			}
			else if(tok->next!=0 && ((tok->next->type==SIMPLE && !isLocalVariable(tok->next->text)) || tok->next->type==PAR_LEFT)) {
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: Command/subcommand next to command/subcommand\n");
				return NULL;
			}
			else {
				CMD *ret_cmd=stackPop2(&cmd_stack);
				if(ret_cmd->nLocal==0) {
					free(ret_cmd->locVar);
					ret_cmd->locVar=NULL;
					free(ret_cmd->locVal);
					ret_cmd->locVal=NULL;
				}
				if(ret_cmd->type==NONE || (ret_cmd->argc<=0 && ret_cmd->type==SIMPLE)) {
					while(!stackEmpty2(&cmd_stack)) {
						freeCMD(stackPop2(&cmd_stack));
					}
					while(!stackEmpty2(&operatorStack)) {
						freeCMD(stackPop2(&operatorStack));
					}
					freeCMD(ret_cmd);
					fprintf(stderr,"Parse: null command\n");
					return NULL;
				}

				return ret_cmd;
				//GGG
			}
		}
		//Case for operator token
		else {
			while(!stackEmpty2(&operatorStack) && hasGreaterPrecedence(operatorStack->value->type,tok->type)) {
				CMD *operator=stackPop2(&operatorStack);
				if(stackEmpty2(&cmd_stack)) {
					fprintf(stderr,"Parse: Not enough operands for operator\n");
					while(!stackEmpty2(&cmd_stack)) {
						freeCMD(stackPop2(&cmd_stack));
					}
					while(!stackEmpty2(&operatorStack)) {
						freeCMD(stackPop2(&operatorStack));
					}
					freeCMD(operator);
					return NULL;
				}
				CMD *operand2=stackPop2(&cmd_stack);
				if(stackEmpty2(&cmd_stack) && (operator->type==PIPE || operator->type==SEP_AND || operator->type==SEP_OR)) {
					freeCMD(operator);
					freeCMD(operand2);
					while(!stackEmpty2(&cmd_stack)) {
						freeCMD(stackPop2(&cmd_stack));
					}
					while(!stackEmpty2(&operatorStack)) {
						freeCMD(stackPop2(&operatorStack));
					}
					fprintf(stderr,"Parse: Not enough operands for operator\n");
					return NULL;
				}
				else if(stackEmpty2(&cmd_stack)) {
					operator->left=operand2; 
					stackPush2(&cmd_stack,operator);
				}
				else {
					CMD *operand1=stackPop2(&cmd_stack);
					if(operator->type==PIPE && operand2->fromFile!=NULL) {
						freeCMD(operand1);
						freeCMD(operand2);
						freeCMD(operator);
						while(!stackEmpty2(&cmd_stack)) {
							freeCMD(stackPop2(&cmd_stack));
						}
						while(!stackEmpty2(&operatorStack)) {
							freeCMD(stackPop2(&operatorStack));
						}
						fprintf(stderr,"Parse: Two input redirects\n");
						return NULL;

					}
					operator->left=operand1;
					operator->right=operand2;

					stackPush2(&cmd_stack,operator);
				}
			}
			if(tok->type==PIPE && redirectedOut) {
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: two output redirects\n");
				return NULL;
			}
			CMD *op_command=mallocCMD();
			op_command->type=tok->type;
			if(stackEmpty2(&cmd_stack)) {
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				freeCMD(op_command);
				fprintf(stderr,"Parse: Null Command\n");
				return NULL;
			}

			stackPush2(&operatorStack,op_command);
			tok=tok->next;
		}
		//**End case for operator tokens
	}
	//stackPrint2(&cmd_stack);
	//Get the rest of the operators through
	while(!stackEmpty2(&operatorStack)) {
		CMD *operator=stackPop2(&operatorStack);
		if(stackEmpty2(&cmd_stack)) {
			freeCMD(operator);
			while(!stackEmpty2(&operatorStack)) {
				freeCMD(stackPop2(&operatorStack));
			}
			fprintf(stderr,"Parse: Not enough operands for operator\n");
			return NULL;

		}
		CMD *operand2=stackPop2(&cmd_stack);
		if(stackEmpty2(&cmd_stack) && (operator->type==PIPE || operator->type==SEP_AND || operator->type==SEP_OR)) {
			freeCMD(operator);
			freeCMD(operand2);
			while(!stackEmpty2(&cmd_stack)) {
				freeCMD(stackPop2(&cmd_stack));
			}
			while(!stackEmpty2(&operatorStack)) {
				freeCMD(stackPop2(&operatorStack));
			}
			fprintf(stderr,"Parse: Not enough operands for operator\n");
			return NULL;
		}
		else if(stackEmpty2(&cmd_stack)) {
			operator->left=operand2;
			stackPush2(&cmd_stack,operator);
		}
		else {
			CMD *operand1=stackPop2(&cmd_stack); 
			if(operator->type==PIPE && operand2->fromFile!=NULL) {
				freeCMD(operand1);
				freeCMD(operand2);
				freeCMD(operator);
				while(!stackEmpty2(&cmd_stack)) {
					freeCMD(stackPop2(&cmd_stack));
				}
				while(!stackEmpty2(&operatorStack)) {
					freeCMD(stackPop2(&operatorStack));
				}
				fprintf(stderr,"Parse: Two input redirects\n");
				return NULL;

			}
			operator->left=operand1; 
			operator->right=operand2;
			stackPush2(&cmd_stack,operator);
		}
	}
	if(numParen!=0) {
		fprintf(stderr,"Parse: mismatched parenthesis\n");
		while(!stackEmpty2(&cmd_stack)) {
			freeCMD(stackPop2(&cmd_stack));
		}
		while(!stackEmpty2(&operatorStack)) {
			freeCMD(stackPop2(&operatorStack));
		}
		return NULL;
	}
	if(cmd_stack->next!=0) {
		while(!stackEmpty2(&cmd_stack)) {
			freeCMD(stackPop2(&cmd_stack));
		}
		while(!stackEmpty2(&operatorStack)) {
			freeCMD(stackPop2(&operatorStack));
		}
		fprintf(stderr,"Parse: Not enough operators.\n");
		return NULL;
	}
	else {
		return stackPop2(&cmd_stack);
	}

}

CMD *parse(token *list) {

	tok=0;
	numParen=0;
	if(list==NULL) {
		return NULL;
	}
	CMD *ret=parse_aux(list);
	numParen=0;
	tok=0;
	return ret;
}
