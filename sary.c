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
#define INIT1 { NULL , 1 }
enum flag 
{ 
	ARROW_RIGHT,
	ARROW_LEFT, 
	ARROW_DOWN,
	ARROW_UP,
	BACKSPACE=127,
	ENTERKEY = 10,
	DEL = 126,
	PgUP,
	PgDn
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
int cy = 1, cx = 1,cx1 = 1, cy1 = 1;
struct termios termios_p,termios_p1,p;
struct winsize ws;
struct editor_buff buff = INIT;
struct track_row_col row_col = INIT1;
void write_buffer_on_screen();
void track_row_column();
void track_row_column1();
void delete_buffer();
int offset_calculate(int x);
void buffer_initializer();
void column_initializer();
void append_buffer_r( char s , int len);
void track_column();
void allocate_column();
void clear_screen();
void position_cursor();
static void sigwinchHandler(int sig);
void denormalizeTerminal();
void backspace_buffer();
void catch_error( char *str,int error_value);
void save_file(char *argv[]);
void exit_terminal(char *argv[]);
void append_buffer( char s , int len);
void get_windows_size();
void normalizeTerminal();
char enter_key(char *argv[]);
void initiate_screen();
void write_rows(char *argv[]);
void buffer_to_window( char *argv[]);
void editor_write( char *argv[]);
int main(int argc , char *argv[])
{	if(isatty(0) && isatty(1) && isatty(2))
	{
		struct sigaction sa;
		allocate_column();
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = sigwinchHandler;
		if (sigaction(SIGWINCH, &sa, NULL) == -1)
			catch_error("sigaction",errno);
		normalizeTerminal();
		editor_write(argv);
	}
	else
		printf("%s\n","file descripter must be associated with unix terminal");
	return 0;
}
void denormalizeTerminal()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_p1) == -1)
		catch_error("tcsetattr",errno);
}
void catch_error( char *str,int error_value)
{
	printf("%s:%s\n",str,strerror(error_value));
	if (buff.str != NULL)
		free(buff.str);
	if (row_col.col != NULL)
		free(row_col.col);
	denormalizeTerminal();
	exit(EXIT_FAILURE);
}
void save_file(char *argv[])
{
	char ch[20];
	char ch1[20];
	FILE *fp;
	write(1,"\x1b[2J",4);
	write(1,"\x1b[3J",4);
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
	if ( fp == NULL )
	{
		fclose(fp);
		catch_error("save_file",errno);
	}
	fwrite(buff.str,1,buff.len,fp);
	fclose(fp);
}
void exit_terminal(char *argv[])
{
	save_file(argv);
	if (buff.str != NULL)
		free(buff.str);
	if (row_col.col != NULL)
		free(row_col.col);
	write(1,"\x1b[2J", 4);
	write(1,"\x1b[H", 3);
	puts("successfully saved\n\r");
	denormalizeTerminal();
	exit(EXIT_SUCCESS);
}

void get_windows_size()
{ 
	if(ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
		catch_error("ioctl",errno);
}
void normalizeTerminal()
{
	if(tcgetattr(STDIN_FILENO, &termios_p) == -1)
		catch_error("tcgetattr",errno);
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
char enter_key(char *argv[])
{
	int nread;
	char c;
	char s[3];
	while(nread = read(STDIN_FILENO, &c, 1)!= 1)
	{
		if (nread == -1 && errno != EAGAIN)
			catch_error("enter_key", errno);
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
				exit_terminal(argv);
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
void initiate_screen()
{
	if(ws.ws_row == 1 && ws.ws_col == 1)
		catch_error("initiate_screen",errno);
	else 
	{
		static int k = 0;
		int c = row_col.row+1;
		if (k == 0)
		{	write(1,"\x1b[2J",4);
			write(1,"\x1b[3J",4);
			k++;
			c = row_col.row;
		}
		char b[10];
		int z = snprintf(b , sizeof(b),	"\x1b[%d;%dH", c ,1);
		write(1,b,z);
		for(int i = c ; i <= ws.ws_row; i++)
		{
			if (i == ws.ws_row)
			{
				write(1,"~",1);
				break;
			}
			write(1,"~\n",2);
		}
	}
}
void write_rows(char *argv[])
{
	write(1,"\x1b[H",3);
	write(1,buff.str,buff.len);
	position_cursor();
	while(1)
	{
		char c = enter_key(argv);
		switch(c)
		{
			case BACKSPACE:
				if ((cx == 1) && (cy == 1))
					break;
			    if ((cx == 1) && (cy > 1))
				{
					cx1= cx;
					cy1 = cy;
					cy -= 1;
					cx = row_col.col[cy-1];
				}
				else
				{
					cx1= cx;
					cy1 = cy;
					cx-=1;
				}
				backspace_buffer();	
				break;
			case DEL:
				cx1= cx;
				cy1 = cy;
				int offset = offset_calculate(cy1);
				if((offset+cx1)>buff.len)
					break;
				delete_buffer();
				break;
			case ARROW_DOWN:
				if(cy < row_col.row)
				{
					cy+=1;
					if (cx > *(row_col.col+cy-1))
					{
						if (cy == row_col.row)
							cx = *(row_col.col+cy-1)+1;
						else
							cx = (*(row_col.col+cy-1)>0)?(*(row_col.col+cy-1)):1;
						position_cursor();
					}
					else
        				position_cursor();
				}
				break;
			case ARROW_UP:
				if (cy > 1)
				{
					cy-=1;
					if (cx > *(row_col.col+cy-1))
					{
						cx = (*(row_col.col+cy-1)>0)?(*(row_col.col+cy-1)):1;
						position_cursor();
					}
					else
						position_cursor();
				}
				break;
			case ARROW_LEFT:
				if (cx > 1)
				{
					cx-=1;
        			position_cursor();
				}
				break;
			case ARROW_RIGHT:
				if (cy == row_col.row) 
				{
					if ((cx < *(row_col.col+cy-1)+1))
					{
						cx+=1;
						position_cursor();
					}
				}
				else if (cx < *(row_col.col+cy-1))
				{
					cx+=1;
        			position_cursor();
				}
				break;
			case ENTERKEY:
				cx1 = cx;
				cy1 = cy;
				char str = '\n';
				if (row_col.row == cy)
				{
					track_column();
					cy += 1;
					row_col.row += 1;
					allocate_column();
				}
				else
				{
					row_col.row += 1;
					allocate_column();
					cy += 1;
					track_column();
				}
				cx = 1;
				append_buffer(str,1);
				break;
			default:
				if (cx == ws.ws_col)
				{
					cx1=cx;
					cy1=cy;
					cx = 1;
					cy += 1;
				}
				else if(cx < ws.ws_col)
				{	
					cx1 = cx;
					cy1 = cy;
					cx += 1;
				}
				append_buffer(c,1);
				break;
		}
	}
}
void buffer_to_window( char *argv[])
{
	char ch;
	char str[2];
	FILE *fp;
	if (argv[1] != NULL)
	{
		if ((fp = fopen(argv[1],"r")) == NULL)
			return;
		else
		{
			while((ch = fgetc(fp))!= EOF)
			{
				append_buffer_r(ch,1);
				if ((ch == '\n') || ((cx == ws.ws_col) && (ch != '\n')))
				{
					row_col.row += 1;
					track_column();
					allocate_column();
					cy += 1;
					cx = 1;
				}
				else
				{
					cx += 1;
					track_column();
				} 
			}
			fclose(fp);
		}
	}
	else
		return;
}
void editor_write( char *argv[])
{
	get_windows_size();
	buffer_to_window(argv);
	initiate_screen();
	write_rows(argv);
}
static void sigwinchHandler(int sig)
{
	if (ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
		exit(EXIT_SUCCESS);
    clear_screen();
	track_row_column1();
	write(STDOUT_FILENO,buff.str,buff.len);
	initiate_screen();
	cx = cx1,cy = cy1;
	position_cursor();
}
void position_cursor()
{
	char buf[10];
	int z = snprintf(buf , sizeof(buf),	"\x1b[%d;%dH", cy,cx);
	write(1,buf,z);
}
void clear_screen()
{
	write(1,"\x1b[2J",4);
	write(1,"\x1b[3J",4);
	write(1,"\x1b[H",3);
}	
void allocate_column()
{
	int *new = realloc(row_col.col,row_col.row*sizeof(int));
	if (new == NULL)
		catch_error("allocate_column",errno);
	row_col.col = new;
	column_initializer();
}
void track_column()
{
	*(row_col.col+cy-1) +=1;
}
void append_buffer_r( char s , int len)
{
	char *new = realloc( buff.str, buff.len + len);
	if (new == NULL)
		return;
	memcpy((new+buff.len),&s,len);
	buff.str = new;
	buff.len += len;
}
void append_buffer( char s , int len)
{
	int offset = offset_calculate(cy1);
	if(offset+cx1 == buff.len+len)
		append_buffer_r(s,len);
	else
	{
		char *new = realloc( buff.str, buff.len + len);
		if (new == NULL)
			return;
		buff.str = new;
		buff.len += len;
		char *buffer1 = (char*)malloc(buff.len-offset-cx1);
		if (buffer1 == NULL)
			catch_error("append_buffer malloc",errno);
		memmove(buffer1,(buff.str+offset+cx1-1),buff.len-offset-cx1);
		*(buff.str +offset+ cx1-1) = s;
		memmove((buff.str+offset+cx1),buffer1,buff.len-offset-cx1);
		free(buffer1);
	}
	write_buffer_on_screen();
}
void backspace_buffer()
{
	int offset = offset_calculate(cy1);
	memmove((buff.str+offset+cx1-2),(buff.str+offset+cx1-1),buff.len-offset-cx1+1);
	buff.len -= 1;
	write_buffer_on_screen();
}
void delete_buffer()
{
	int offset = offset_calculate(cy1);
	memmove((buff.str+offset+cx1-1),(buff.str+offset+cx1),buff.len-offset-cx1+1);
	buff.len -= 1;
	write_buffer_on_screen();
}
void column_initializer()
{
	*(row_col.col+row_col.row-1) = 0;
}
int offset_calculate(int x)
{
	int offset = 0;
	for(int i = 0; i < (x-1); i++)
		offset = offset + row_col.col[i];
	return offset;
}
void track_row_column()
{
	cx=1,cy=1;
    row_col.col = (int*)malloc(1);
	row_col.row=1;
	allocate_column();
	for (int i = 0; i < buff.len; i++)
	{
		if ((*(buff.str+i) == '\n') || ((cx == ws.ws_col) && (*(buff.str+i) != '\n')))
		{
			row_col.row += 1;
			track_column();
			allocate_column();
			cy += 1;
			cx = 1;
		}
		else
		{   
			cx += 1;
			track_column();
		} 
	}
}
void write_buffer_on_screen()
{
	int a = cx,b = cy;
	clear_screen();
	track_row_column();
	write(STDOUT_FILENO,buff.str,buff.len);
	initiate_screen();
	cx = a, cy = b;
	position_cursor();
}
void track_row_column1()
{
	int n = offset_calculate(cy)+cx;
	int m,j;
	track_row_column();
	for(int i = 1; i <= row_col.row; i++)
	{
		m = offset_calculate(i);
		for (int j = 1; j <= row_col.col[i-1];j++)
		{
			if((m+j) == n )
			{
				cx1=j;
				cy1=i;
				break;
			}
		}
		if((m+j) == n )
			break;
	}
}