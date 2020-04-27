#include<termios.h>
#include<stdio.h>
#include<errno.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/ioctl.h>
#include<signal.h>
#define _GNU_SOURCE
#define ESC 27
#define CTRL_KEY(k) ((k) & 0x1f)
#define INIT { NULL , 0 }
#define INIT1 { NULL ,1 }
enum flag 
{ 
	ARROW_RIGHT,
	ARROW_LEFT, 
	ARROW_DOWN,
	ARROW_UP,
	BACKSPACE=127,
	ENTERKEY = 10,
	DEL = 126
};
struct track_row_col
{
	int *col;
	int row;
};
struct editor_buff 
{
	char *str;
	int len;
};
struct track_row_col row_col = INIT1;
int cy = 1, cx = 1;
int cx1 = 1, cy1 = 1;
int offset;
struct termios termios_p;
struct termios termios_p1;
struct termios p;
struct winsize ws;
void track_column(struct editor_buff *buf1);
void allocate_column(struct editor_buff *buf1);
void clear_screen();
void position_cursor();
static void sigwinchHandler(int sig);
void denormalizeTerminal(struct editor_buff *buff1);
void catch_error( char *str,int error_value,struct editor_buff *buff1);
void save_file(struct editor_buff *buff1,char *argv[]);
void exit_terminal(struct editor_buff *buff1,char *argv[]);
void append_buffer(struct editor_buff *buff1,const char *s , int len);
void get_windows_size(struct editor_buff *buff1);
void normalizeTerminal(struct editor_buff *buff1);
char enter_key(struct editor_buff *buff1,char *argv[]);
void initiate_screen(struct editor_buff * buff1);
void write_rows(struct editor_buff *buff1,char *argv[]);
void buffer_to_window(struct editor_buff *buf1, char *argv[]);
void editor_write(struct editor_buff *buf1, char *argv[]);
int main(int argc , char *argv[])
{	
	struct editor_buff buff1 = INIT;
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigwinchHandler;
	if (sigaction(SIGWINCH, &sa, NULL) == -1)
		catch_error("sigaction",errno,&buff1);
	normalizeTerminal(&buff1);
	editor_write(&buff1,argv);
	return 0;
}
void denormalizeTerminal(struct editor_buff *buff1)
{
	 if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_p1) == -1)
		 catch_error("tcsetattr",errno,buff1);
}
void catch_error( char *str,int error_value,struct editor_buff *buff1)
{
	printf("%s:%s\n",str,strerror(error_value));
	free(buff1->str);
	denormalizeTerminal(buff1);
	exit(EXIT_FAILURE);
}
void save_file(struct editor_buff *buff1,char *argv[])
{
	char ch[20];
	char ch1[20];
	FILE *fp;
	write(1,"\x1b[2J",4);
	write(1,"\x1b[H",3);
	if (argv[1] == NULL)
	{
		printf("%s","enter the file name:");
		scanf("%[^\n]s",ch);
		fp = fopen(ch,"w");
	}
	else
	{
		strcpy(ch1,argv[1]);
		fp = fopen(ch1,"w");
	}
	if ( fp == NULL)
	{
		fclose(fp);
		catch_error("save_file",errno,buff1);
	}
	fwrite(buff1->str,1,buff1->len,fp);
	fclose(fp);
}
void exit_terminal(struct editor_buff *buff1,char *argv[])
{
	save_file(buff1,argv);
	free(buff1 -> str);
	write(1,"\x1b[2J", 4);
	write(1,"\x1b[H", 3);
	puts("successfully saved\n\r");
	denormalizeTerminal(buff1);
	exit(EXIT_SUCCESS);
}
void append_buffer(struct editor_buff *buff1,const char *s , int len)
{
	if ((strcmp(s,"\n") == 0) && (cx1 <= row_col.col[cy1-1]))
	{
		char *new = realloc( buff1 -> str, buff1 ->len + len);
		if (new == NULL)
			return;
		//memcpy(&new[buff1 -> len],s,len);
		buff1 -> str = new;
		buff1 -> len += len;
	//	char *buffer1 = (char*)malloc(offset+1);
		char *buffer2 = (char*)malloc(buff1->len);
		//memcpy(buffer1,buff1->str,offset-1);
		//strncpy(buffer1,buff1->str,offset);
		strcpy(buffer2,(buff1->str+offset-1));
		*(buff1 -> str + offset-1) = '\n';
		strcpy((buff1->str+offset),buffer2);
	//	position_cursor();
		clear_screen();
		write(STDOUT_FILENO,buff1->str,buff1->len+1);
	}
	else
	{
		char *new = realloc( buff1 -> str, buff1 ->len + len);
		if (new == NULL)
			return;
		memcpy((new+buff1->len),s,len);
		buff1 -> str = new;
		buff1 -> len += len;
	}	
}
void get_windows_size(struct editor_buff *buff1)
{ 
	if(ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
		catch_error("ioctl",errno,buff1);
}
void normalizeTerminal(struct editor_buff *buff1)
{
	if(tcgetattr(STDIN_FILENO, &termios_p) == -1)
		catch_error("tcgetattr",errno,buff1);
	termios_p1 = termios_p;
	p = termios_p;
	termios_p.c_lflag &=~(ICANON | ISIG | IEXTEN | ECHO);
	termios_p.c_iflag &=~(BRKINT | INPCK | ISTRIP | IXON );
//	termios_p.c_oflag &=~(OPOST | ONLCR);     
	termios_p.c_oflag |= (OPOST | ONLCR );
	termios_p.c_cc[VMIN] = 1;
	termios_p.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH , &termios_p);
}
char enter_key(struct editor_buff *buff1,char *argv[])
{
	int nread;
	char c;
	char s[3];
	while(nread = read(STDIN_FILENO, &c, 1)!= 1)
	{

		if (nread == -1 && errno != EAGAIN)
			catch_error("enter_key", errno,buff1);
	}
	while(1)
	{
		switch(c){
			case ESC:
				if (read(1,s,1)==0)
					return 0;
				if (read(1,s+1,1) == 0)
					return 0;
				else 
				{
					switch(s[1])
					{
						case 'A':
							return ARROW_UP;
							break;
						case 'B':
							return ARROW_DOWN;
							break;
						case 'C':
							return ARROW_RIGHT;
							break;
						case 'D':
							return ARROW_LEFT;
							break;
					}
				}
				if(read(1,s+2,1) == 1)
					if (s[2] == '~')
						return s[2];
			case  CTRL_KEY('q'):
				exit_terminal(buff1,argv);
				break;
			case BACKSPACE:
			case CTRL_KEY('h'):
				return BACKSPACE;
				break;
			case ENTERKEY:
				return ENTERKEY;
				break;
			}	
	return c;
	}
}
void initiate_screen(struct editor_buff * buff1)
{
	if(ws.ws_row == 1 && ws.ws_col == 1)
		catch_error("initiate_screen",errno,buff1);
	else 
	{
		write(1,"\x1b[2J",4);
		write(1,"\x1b[H",3);
		for(int i = 1; i <= ws.ws_row; i++)
			{
				write(1,"~\n",2);
				if (i == ws.ws_row)
					write(1,"~",1);
			}
	}
}
void write_rows(struct editor_buff *buff1,char *argv[])
{
	write(1,"\x1b[H",3);
	write(1,buff1->str,buff1 ->len);
	position_cursor();
	while(1)
	{
		char c = enter_key(buff1,argv);
		char str[2];
		switch(c)
		{
			case BACKSPACE:
			case DEL:
				if ((cx == 1) && (cy == 1))
					break;
				if ((cx == 1) && (cy > 1))
				{
					cy -= 1;
					cx = ws.ws_col;
        			position_cursor();
				}
				else
				{
					cx-=1;
					position_cursor();
					write(1,"\x1b[0K",4);
					//append_buffer(buff1,"\b",1);
				}
				break;
			case ARROW_DOWN:
				if(cy < row_col.row)
					cy+=1;
        		position_cursor();
				break;
			case ARROW_UP:
				if (cy > 1)
					cy-=1;
        		position_cursor();
				break;
			case ARROW_LEFT:
				if (cx > 1)
					cx-=1;
        		position_cursor();
				break;
			case ARROW_RIGHT:
				if (cx < (row_col.col[cy-1]+1))
					cx+=1;
        		position_cursor();
				break;
			case ENTERKEY:
				if (cy < ws.ws_row)
				{ 
					cx1 = cx;
					cy1 = cy;
					cy += 1;
					cx = 1;
					char str[2];
					str[0] = '\n';
					str[1] = '\0';
					row_col.row += 1;
					write(1,str,strlen(str));
					append_buffer(buff1,"\n",1);
				}
				break; 
			default:
				if (cx < ws.ws_col)
					cx+=1;
				else if ((cx = ws.ws_col) && (cy < ws.ws_row))
				{
					cx = 1;
					cy += 1;
				}
				str[0] = c;
				str[1] = '\0';
				write(1,str,strlen(str));
				append_buffer(buff1,str,strlen(str));
				track_column(buff1);
				break;
		}
	}
}
void buffer_to_window(struct editor_buff *buf1, char *argv[])
{
	char ch;
	char str[2];
	FILE *fp;
	if (argv[1] != NULL)
	{
		
		if ((fp = fopen(argv[1],"r")) == NULL)
		{
			return;
		}
	}
	else
	{
		return;
	}
	while((ch = fgetc(fp))!= EOF)
	{
		str[0] = ch;
		str[1] = '\0';
		append_buffer(buf1,str,strlen(str));
		if (ch = '\n')
		{
			row_col.row += 1;
		}
		else
		{
			track_column(buf1);
		} 
	}
	fclose(fp);
}
void editor_write(struct editor_buff *buf1, char *argv[])
{
	get_windows_size(buf1);
	initiate_screen(buf1);
	buffer_to_window(buf1,argv);
	write_rows(buf1,argv);
}
static void sigwinchHandler(int sig)
{
	if (ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
		exit(EXIT_SUCCESS);
}
void position_cursor()
{
	char buf[32];
	offset = cx;
	int z = snprintf(buf , sizeof(buf),	"\x1b[%d;%dH", cy,cx);
	write(1,buf,z);
}
void clear_screen()
{
	write(1,"\x1b[2J",4);
	write(1,"\x1b[H",3);
}	
void allocate_column(struct editor_buff *buf1)
{
	int *new = realloc(row_col.col,row_col.row*sizeof(int));
	if (new == NULL)
		catch_error("allocate_column",errno,buf1);
	row_col.col = new;
}
void track_column(struct editor_buff *buf1)
{
	allocate_column(buf1);
	row_col.col[row_col.row-1] +=1;
}