// UCLA CS 111 Lab 1 command reading

#include "c-stack.h"
#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <string.h>
#include <stdio.h>
#include "ctype.h"
#include <stdlib.h>
struct command_stream 
{
  int numCommands;
  unsigned int currentStreamSize;
  int currentCommand;
  command_t stream[];
};

enum
{
    AND, OR, PIPE, LEFTP, RIGHTP, SEMIC, NEWLINE
};

int g_lineNumber = 0;

/**
 * Creates an array of cstrings (char*) to words of the command
 * (delimited by spaces)
 * Ex. echo hello
 * char** 0 -> (char* -> echo)
 *        1 -> (char* -> hello)
 */
char** createWordArray(char* words)
{
  int numWords = 5, i = 0, j = 0, wordNum = 0; // prediction
  unsigned int wordArraySize = sizeof(char*) * numWords;
  char** wordArray = checked_malloc(wordArraySize);
  memset(wordArray, 0, wordArraySize);

  while(words[i] != '\0' && words[j]) {
    while(words[j] != ' ' && words[j] != '\0') {
      j++;
    }
    // Now, word[j] is a space, copy i till j-1 into the first word
    int numChars = j - i;
    char* word = checked_malloc(sizeof(char) * numChars + 1);
    memcpy(word, &words[i], sizeof(char) * numChars);
    word[numChars] = '\0'; // finish the word
    wordArray[wordNum] = word;
    if (words[j] == '\0') {
      break; // we are done;
    } else {
      i = ++j;
      wordNum++;
      if((sizeof(char*) * wordNum) >= wordArraySize) {
        // We have more words than the array can currently hold
        wordArraySize *= 2;
        wordArray = checked_realloc(wordArray, wordArraySize);
      }
    }
  }
  //printf("Created word array for %s\n", words);
  return wordArray;
}

// Extracts the word, and input/output(if applicable) into the command
void extractWordInputOutput(char* currentWord, command_t command)
{
  int i = 0, j = 0, expectAnotherRedir = 0;
  int currentWord_size = strlen(currentWord);
  int output_size, input_size;

  command->type = SIMPLE_COMMAND;
  switch(isRedirectionCommand(currentWord))
  {
   // case 2: //TODO ERASE THIS LINE!!!! THIS WILL BREAK SHIT
    case 0: // this command is not a redirection command.
      command->u.word = createWordArray(currentWord);
      break;
    case 2: // this command has two redirections
      expectAnotherRedir = 1;
    case 1:// this command has one redirection
      while(currentWord[i] != '\0')
      {
        if(currentWord[i] == '>')
        {
          output_size = currentWord_size - i;// includes null byte
          char *outputBuffer = checked_malloc(sizeof(char)*output_size);
          strcpy(outputBuffer, &currentWord[i+1]);

          command->output = outputBuffer;
    
          currentWord[i] = '\0';
          command->u.word = createWordArray(currentWord);
          break;
        }
        else if(currentWord[i] == '<')
        {
          if(expectAnotherRedir == 0)
          {
            input_size = currentWord_size - i;//includes null byte
            char *inputBuffer = checked_malloc(sizeof(char)*input_size);
            strcpy(inputBuffer, &currentWord[i+1]);
          
            command->input = inputBuffer;
    
            currentWord[i] = '\0';
            command->u.word = createWordArray(currentWord);
          }
          else
          {
            j = i + 1;
            while(currentWord[j] != '>')
            {
              if(currentWord[j] == '\0')
                error(1,0,"%i: Syntax error. Ambiguous redirections.", g_lineNumber);
              j++;
            }
 
            input_size = j - i; // includes null byte
            output_size = currentWord_size - j;
             
            char *inputBuffer = checked_malloc(sizeof(char)*input_size);
            char *outputBuffer= checked_malloc(sizeof(char)*output_size);

            strncpy(inputBuffer,&currentWord[i+1], input_size - 1);
            inputBuffer[input_size - 1] = '\0'; 
            strcpy(outputBuffer, &currentWord[j+1]);
 
            command->output = outputBuffer;
            command->input = inputBuffer;

            currentWord[i] = '\0';
            command->u.word = createWordArray(currentWord);
          }
          break;
        }
        i++;
      }
      break;
    default: //throw error here
      break;
 
  }
}

int getRidOfExtraWhitespaces(char* word)
{
  //printf("String to delW %s\n",word);
  int i = 0, j = 0;
  while(word[i] != '\0')
  { 
    if(word[i] == ' ' && (word[i+1] == ' ' || word[i+1] == '\0' || i == 0 || word[i+1] == '<' || word[i+1] == '>'))
    {
      j = i;
      while(word[j] != '\0')
      {
        word[j] = word[j+1];
        j++;
      }
    }
    else if ((word[i] == '<' || word[i] == '>') && word[i+1] == ' ')
    {
      j = i + 1;
      while(word[j] != '\0') 
      {
        word[j] = word[j+1];
        j++;
      }
    }
    else
    {
      i++;
    }
  }

  //as a hack, make sure buffer has more than just whitespaces.
  if( word[0] == ' ' || word[0] == '\0')
  {
    return 1;
  }
  
  return 0;
}

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
         void *get_next_byte_argument)
{ 
  char *operators[] = { "&&", "||", "|", "(", ")", ";" };//, "\n" };
  int precedences[] = {   1,    1,   2,  4,   -1,   0, 0 };

  int i, j, isOperator, numChars = 0, inComment = 0;
  int operatorNumber = 0;
  int possibleNewCommand = 0;
  unsigned int currentWordSize = 16 * sizeof(char);

  int lastOperatorWasRightP = 0;
  char *operand, *lastCommand, *currentWord = checked_malloc(16 * sizeof(char));
  char c = (char) get_next_byte(get_next_byte_argument);

  command_t operand1, operand2;

  // Initialize command stream
  command_stream_t commandStream = checked_malloc(10 * sizeof(struct command));
  commandStream->currentStreamSize = 10 * sizeof(struct command);
  commandStream->numCommands = 0;
  commandStream->currentCommand = 0;

  g_lineNumber = 0;

  init_stacks();
  while(c != EOF) 
  {
    //printf("loop entered\n");
    if( c == '\n') 
    {
      g_lineNumber++;
    }

    if(inComment)
    {
      if( c == '\n' ) 
      {
        inComment = 0;
        c = get_next_byte(get_next_byte_argument);
        continue;
      }
      else
      {
        c = get_next_byte(get_next_byte_argument);
        continue;
      }
    }
    else if( c == '#' )
    {
      inComment = 1;
      c = get_next_byte(get_next_byte_argument);
      continue;
    }

    /**
     * Basically if there is a non-operator followed by a newline, there is a possibility
     * that the current command is done. The current command will be over if the newline
     * is then followed by another non-operator. Also ignore newlines inside commands.
     */

    //printf("possible New COmmand is set to %i\n",possibleNewCommand);
    if ( (!isOperator || lastOperatorWasRightP)  && c == '\n') 
    {
      possibleNewCommand = 1;
      //printf("PossibleNewCommand set to 1 char is %c\n",c);
      // Used if a new command is actually started to finish the current one
      lastCommand = currentWord;
      fpos_t pos;
      fgetpos(get_next_byte_argument, &pos);
      c = get_next_byte(get_next_byte_argument);
      if (c != '\n') 
      {
        //printf("continuing after setting possibleNewCommand to 1\n");
       // if(c == EOF)
        // printf("new character is EOF and num of operands left is %i\n",g_iOperand_stack_size);
        fsetpos(get_next_byte_argument, &pos);
        lastOperatorWasRightP = 0;
        continue;
      }
      else {
        g_lineNumber++;
      }
    } 
    else if (c == '\n') 
    {
      c = get_next_byte(get_next_byte_argument);
      //printf("Operator followed by newlien detected. continiuing\n");
      
      lastOperatorWasRightP = 0;
      continue;
    }
    //printf("Checkpoint 2\n");
    /**
     * This loop checks if the current char is an operator. If it is, we create the operator word
     * and then we check the next char in case of double char operators such as || or &&
     */
    lastOperatorWasRightP = 0;
    isOperator = 0;
    for (i = 0; i < 6; i++) 
    { // check if start of an operator
      if (c == operators[i][0]) 
      {
        isOperator = 1;
        operand = currentWord; // Save old operand before creating operator
        currentWord = checked_malloc(3 * sizeof(char));
        // Create the operator word
        currentWord[0] = c;
        fpos_t pos;
        fgetpos(get_next_byte_argument, &pos);
        
    
        if (c == '|' || c == '&') 
        {
          if(g_iOperand_stack_size <= 0 && numChars <= 0)
            error(1,0,"%i: Did not expect binary operator",g_lineNumber);
          
          char current_char = c;
          c = get_next_byte(get_next_byte_argument);
          currentWord[1] = c;
          currentWord[2] = '\0';
          if (c == '|' && c == current_char) 
          {
            i = OR;
          } 
          else if (c == '&' && c == current_char) 
          {
            i = AND;
          }
          else if( current_char == '&' && c != current_char)
          {
            //TODO: Throw syntax error here. Single &!
            //fprintf(stderr,"%i: Isolated & found. Not a valid word character, and it is not an &&",lineNumber);
            error(1,0,"%i: Isolated & found",g_lineNumber);
          }
          else 
          {
            i = PIPE;
            currentWord[1] = '\0';
            fsetpos(get_next_byte_argument, &pos);
          }
        }
        else if( c == '(')
        {
          //printf("Found leftP\n");
          i = LEFTP;
          currentWord[1] = '\0';
          fsetpos(get_next_byte_argument, &pos);
        } 
        else if( c == ')')
        {
          //printf("Found rightP\n");
          lastOperatorWasRightP = 1;
          i = RIGHTP;
          currentWord[1] = '\0';
          fsetpos(get_next_byte_argument, &pos);
        }
        else if( c == ';')  
        {  
          if(g_iOperand_stack_size <= 0 && numChars <= 0)
            error(1,0,"%i: Did not expect binary operator",g_lineNumber);
          currentWord[1] = '\0';
          fsetpos(get_next_byte_argument, &pos);
        }
        else
        {
          currentWord[1] = '\0';
          fsetpos(get_next_byte_argument, &pos);
        }
        break;
      }
    }

    /**
     * When we encounter an operator, we can create a simple command out of the previous word
     * if there was a word (operand) there. This should always happen??
     */
    if (isOperator) 
    {
      //printf("isOperator\n");
      if (numChars > 0) 
      {
        operand[numChars] = '\0';
        //printf("Pushed %s on operand stack\n", operand);
        //get rid of whitespaces in operand here.
        int onlyWhite = getRidOfExtraWhitespaces(operand);
        if(onlyWhite == 0)
          push_operand(createSimpleCommand(operand));
        // printf("simple out of operand %s\n", operand);
        numChars = 0;
      }

      /**
       * While the top of the stack contains an operator with equal or higher precedance than the
       * operator we are currently dealing with, keeping popping one operator and two operands,
       * creating a command out of them, and pushing it onto the operand stack.
       */
      //printf("Charly:top of stack is %i\n",operator_stack_top());
     // if(operator_stack_top() == RIGHTP ) //eval stack until a LEFTP is found
      if(i == RIGHTP)
      {
        //printf("EvalStackuntilLeftP found. possibleNewCommand is %i\n",possibleNewCommand);
        evalStackUntilLeftP();
        //printf("current char is %c\n",c);
        c = get_next_byte(get_next_byte_argument);
        //printf("nextchar is %c\n",c);
        continue;
      }
      while(g_iOperator_stack_size > 0 && g_iOperand_stack_size > 1 
          && (precedences[operator_stack_top()] >= precedences[i]) 
          && ((operator_stack_top() != LEFTP) && i != RIGHTP )) 
      {
        operand2 = pop_operand();
        operand1 = pop_operand();
        operatorNumber = pop_operator();
        //printf("popped operands types %d %d\n", operand1->type, operand2->type);
        //printf("popped operator %s\n", operators[operatorNumber]);
        //printf("pushed type %d operand on stack\n", operatorNumber);
        push_operand(createCommand(operand1, operand2, operatorNumber));
      }

      // Get the operator number for the current word
      for (j = 0; j < 6; j++)
      {
        if (strcmp(currentWord, operators[j]) == 0)
        {
          break;
        }
      }
      //printf("pushed operators %s %d on stack\n", currentWord, j);
      push_operator(j);
      currentWord = checked_malloc(3 * sizeof(char));
    } 
    else if (!possibleNewCommand) 
    {
      //if last operator was rightP. do not run this.
      //printf("not a new command. new char is %c, numChars is %i, currentWordSize is %i\n",c,numChars,currentWordSize);
      if(c != ' ' || numChars > 0)
      {
        //printf("growing current word %s currentWordSize is %i and numChars is %i\n",currentWord,currentWordSize,numChars);
        // Grow current word which is an operand if necessary
        if ((numChars * sizeof(char)) >= currentWordSize)
        {
          //printf("doubling size of word %s\n",currentWord);
          currentWordSize *= 2;
          //char * buffer = checked_malloc(currentWordSize);
          //strncpy(buffer,currentWord,numChars);
          //free((void *)currentWord);
          //currentWord = checked_malloc(currentWordSize);
          //strncpy(currentWord,buffer,numChars);
          currentWord = checked_realloc(currentWord, currentWordSize );// for some reason this was messing up
          //printf("it is now %s\n",currentWord);
        }
        currentWord[numChars] = c;
        numChars++;
      }
    } 
    else 
    {
      //printf("Going to new command. operand stack size is %i\n",g_iOperand_stack_size);
      //printf("Operator stack size is %i. numChars is %i\n",g_iOperator_stack_size,numChars);
       
      //if(g_iOperand_stack_size <= 0)
      //{
       // c =  get_next_byte(get_next_byte_argument);
       // possibleNewCommand = 0;
        //continue;
      //} 
      /**
       * This means that we are about to go onto a new command. We stop the current command and 
       * finish building it with the lastCommand variable, and an operand/operator if necessary.
       * This command is then put into the command stream.
       */
      lastCommand[numChars] = '\0';
      //printf("Last Command getting rid of whites\n"); 
      if(numChars > 0)
      {
        //printf("numchars is > 0 so word is %s\n",lastCommand);
        getRidOfExtraWhitespaces(lastCommand);
        push_operand(createSimpleCommand(lastCommand));
      }
      else if(g_iOperand_stack_size <= 0)
      {
        c =  get_next_byte(get_next_byte_argument);
        possibleNewCommand = 0;
        continue;
      } 

      //else
      //{
       // printf("numChars == 0 so we are continuing\n");
        //continue
       // c = get_next_byte(get_next_byte_argument);
       // possibleNewCommand = 0;
       // continue;
      //}
      evalStack();
      //printf("%s\n", "Finished one command");

      if ((commandStream->numCommands * sizeof(struct command)) == commandStream->currentStreamSize)
      {
        commandStream->currentStreamSize *= 2;
        commandStream = checked_realloc(commandStream, commandStream->currentStreamSize);
      }
      command_t commandToPushToStream = pop_operand();
      commandToPushToStream->status = -1;
      commandStream->stream[commandStream->numCommands] = commandToPushToStream;
      // TODO: if stack not empty, there was an error?
      commandStream->numCommands++;
      numChars = 0;
      currentWord = checked_malloc(3 * sizeof(char)); // prevent overwriting
    }
    //printf("PossibleNewCOmmand set to 0 right before checkpoint1\n");
    c = get_next_byte(get_next_byte_argument);
    possibleNewCommand = 0;
  }

  //printf("Checkpoint1. possibleNEwCommand is %i num of operands is %i\n",possibleNewCommand,g_iOperand_stack_size);
  // Push last word onto operand stack and evaluate the rest
  if (!isOperator) 
  {
    currentWord[numChars] = '\0';
    //printf("pushed simple word %s with word count %i strlen is %i\n", currentWord,numChars,(int)strlen(currentWord));
    getRidOfExtraWhitespaces(currentWord);
    if(strlen(currentWord) > 0)
      push_operand(createSimpleCommand(currentWord));
  } else {
    // if a semicolon, valid?
  }
  //printf("evalstack at the end of loop\n");
  evalStack();
  // Put last command in command stream
  // if there is one!
  if(operand_stack_top() != NULL)
  {
    command_t commandToPushToStream = pop_operand();
    commandToPushToStream->status = -1;
    commandStream->stream[commandStream->numCommands] = commandToPushToStream;
    commandStream->numCommands++;
  }
  //printf("Stack sizes: %d, %d\n", g_iOperator_stack_size, g_iOperand_stack_size);
  //printf("Final command type is %d\n", commandStream->stream[0]->type);

  return commandStream;
}

command_t createSimpleCommand(char* currentWord)
{
  //printf("creating simple command with word %s\n",currentWord);
  if (isValidWordCharacter(currentWord) == 0)
    error(1,0,"%i: Invalid word character",g_lineNumber);
 
  command_t simpleCommand = checked_malloc(sizeof(struct command));

  extractWordInputOutput(currentWord, simpleCommand);
  //if(simpleCommand->input != NULL || simpleCommand->output != NULL)
  // printf("simpleCommand word is %s\n", currentWord);
  return simpleCommand;
}

command_t createCommand(command_t operand1, command_t operand2, int operatorNumber)
{
  command_t newCommand = checked_malloc(sizeof(struct command));

  switch(operatorNumber) {
    case AND:
      newCommand->type = AND_COMMAND;
      newCommand->u.command[0] = operand1;
      newCommand->u.command[1] = operand2;
      break;
    case OR:
      newCommand->type = OR_COMMAND;
      newCommand->u.command[0] = operand1;
      newCommand->u.command[1] = operand2;
      break;
    case PIPE:
      newCommand->type = PIPE_COMMAND;
      newCommand->u.command[0] = operand1;
      newCommand->u.command[1] = operand2;
      break; 
    case SEMIC: 
      if(operand1 == NULL)
      {
        operand1 = operand2;
        operand2 = NULL;
      }
      newCommand->type = SEQUENCE_COMMAND;
      newCommand->u.command[0] = operand1;
      newCommand->u.command[1] = operand2;
      break;
    case LEFTP:
    case RIGHTP:
      newCommand->type = SUBSHELL_COMMAND;
      newCommand->u.subshell_command = operand1;
      break;
    
  }
  return newCommand;
}

/**
 * While there are still operators, keep evaluating the stack so that what is left will
 * be one complete command in the operand stack.
 */
void evalStack()
{
  command_t operand2, operand1;
  int operatorNumber;

  while(g_iOperator_stack_size > 0) 
  {
    operand2 = pop_operand();
   // if( !( g_iOperand_stack_size <= 0 && operator_stack_top() == SEMIC) )
      operand1 = pop_operand();
   // else //allow ';' to only have one operand
   //   operand1 = NULL;
    operatorNumber = pop_operator();
    push_operand(createCommand(operand1, operand2, operatorNumber));
  }
  if(g_iOperand_stack_size > 1)
  {
    error(1, 0, "%i: Syntax Error. Incorrect number of elements on operand stack after evalStack\n", g_lineNumber);
  }
}

void evalStackUntilLeftP()
{
  //printf("eval leftp called\n");
  command_t operand2, operand1;
  int operatorNumber;
  
  //pop_operator(); // get rid of RIGHTP
  while(operator_stack_top() != LEFTP)
  {
    //printf("operator top is %i\n",operator_stack_top());
    if(operator_stack_top() == -1)
      error(1,0,"%i: Syntax Error. Mismatching parenthesis\n",g_lineNumber);

    operand2 = pop_operand();
    operand1 = pop_operand();
    operatorNumber = pop_operator();
    push_operand(createCommand(operand1, operand2, operatorNumber));
  }
  //do not assume we have a result. Parenthesis could be empty ().
  pop_operator();// get rid of LEFTP
  //printf("Operator_stackTop %i\n",operator_stack_top());
  //printf("Operand_stacktop %i\n",operand_stack_top());
  if(operand_stack_top() != NULL)
  {
    //convert command at the top of operand stack to subshell command.
    //printf("pooping operand\n");
    operand1 = pop_operand(); 
    push_operand(createCommand(operand1, NULL, RIGHTP));
  }
  //printf("Number of operands left %i\n",g_iOperand_stack_size);
   //printf("Number of operators left %i\n",g_iOperator_stack_size);
}

void checkForConsecutiveRedir(char* word)
{
  int i = 0;
  int j = 0;
  int wordStarted = 0;

  while(word[i] != '\0')
  {
    if(word[i] == '>' || word[i] == '<')
    {
      
      j = i + 1;
      if(wordStarted == 0 || word[j] == '\0') // redirects must be followed by a word
      {
        error(1,0,"%i: Syntax Error. Bad redirection.", g_lineNumber);
      }
      else // if we found a > or < we are no longer inside of a word
      {
        wordStarted = 0;
      }
      while(word[j] != '\0') // keep checking after redirect
      { 
        if(word[j] == ' ') // ignore spaces
        {
          j++;
          continue;
        }
        else if( word[j] == '<' || word[j] == '>' || word[j] == '\0') // if this comes before we actually find a good character, error
        {
          error(1,0,"%i: Syntax Error. Bad redirection.", g_lineNumber);
        }
        else 
        {
          break;
        }
      }  
    }
    else if (word[i] != ' ')
    {
      wordStarted = 1;
    }
     
    i++;
  }
}

int isValidWordCharacter(char* word)
{
  int i = 0;

  checkForConsecutiveRedir(word);

  while(word[i] != '\0')
  {
    if(isdigit(word[i]) || isalpha(word[i]))
    {
      i++;
      continue;
    }

    switch(word[i])
    {
    case '<'://not really
    case '>'://not really
    case '!':
    case '%':
    case '+':
    case ',':
    case '-':
    case '.':
    case '/':
    case ':':
    case '@':
    case '^':
    case '_':
    case ' '://not really
      i++;
      continue;
    default:
      return 0;
    }
  }
  
  return 1;
}

//returns 0 if not, otherwise returns the number of redirection characters
int isRedirectionCommand(char* word)
{
  int i = 0;
  int count = 0;
  while(word[i] != '\0')
  {
    if(word[i] == '<' || word[i] == '>')
      count++;
    i++;
  }

  return count;
}


command_t
read_command_stream (command_stream_t s)
{
  if (s->currentCommand == s->numCommands)
  {
    return NULL;
  }
  command_t command = s->stream[s->currentCommand];
  s->currentCommand++;
  return command;
}
