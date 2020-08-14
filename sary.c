/*OVERVIEW OF CODE HOW ACTUALLY IT WORKS:
Terminal attributes are defined in termios structure which comes under <termios.h> header file.
Certain attributes of terminal are modified using tcgetattr() and tcsetatter() to enable raw input mode in terminal.
A buffer is defined of type struct editor_buff. Whatever alphanumeric keys,symbol written on editor,all are stored in the
form of character in buffer named 'buff' in respective sequential position where the key is pressed in terminal window.
The buffer is updated whenever any of key is typed  and then terminal screen is cleared using escape 
sequence command and then the updated buffer which is string buffer is printed on terminal screen on each keystroke.
 */
/*mouse scrolling and touch pad scrolling is not implemented.i am working on it.
scrolling can be performed with arrow key UP_ARROW and DOWN_ARROW ,PgUp and PgDn.
Horizontal scrolling is not implemented.While implementing scrolling,as the buffer is string buffer named 'buff',the
substring of buffer 'buff' is selected in such a way that it fits the terminal window and respective substring is dis-
played on terminal window whenever ARROW_UP,ARROW_DOWN,PgUp,PgDn is pressed. Substring is selected with help of appro-
priate value of two index variable 'tindex','bindex' which means topindex and bottomindex respectively*/
#include<termios.h>
#include<stdio.h>
#include<errno.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/ioctl.h>
#include<signal.h>
#include<stdbool.h>
#include<time.h>
#include<ctype.h>
#define _GNU_SOURCE
#define CTRL_KEY(k) ((k) & 0x1f)
#define INIT { NULL , 0 }
#define INIT1 { NULL , 1 }
enum flags { 
    NUL=0,ESC=27,BACKSPACE=127,ENTERKEY = 10,TAB = 9,ARROW_UP=1000,ARROW_DOWN,ARROW_RIGHT,ARROW_LEFT,PgUP,PgDn,DEL,HOME,END,INS,SAVE
};
/*termios structure defines the terminal attributes*/
struct termios termios_p,termios_p1,p;
/*winsize structure records the terminal window size*/
struct winsize ws;
/*structure for tracing the row and column  of recent character with respect to the terminal window size*/
struct track_row_col{
    int *col;
    int row;
};
/*structure for string buffer to record the typed characters*/
struct editor_buff {
    char *str;
    long len;
};
struct editor_buff buff = INIT;
struct track_row_col row_col = INIT1;
int cy = 1, cx = 1,cx1 = 1, cy1 = 1,y=1,x=1,lenwbs,tindex,bindex;//bindex and tindex refers to bottom index and top index of buffer.it is used while scrolling.
char ch[20];
bool INS_mode = false; //to detect whether insert key is pressed or not. by default INS_mode is  false which indi
                       //cates that insert key is not pressed initially.
void denormalizeTerminal(); //disables raw input mode and brings terminal to default mode.
void catch_error( char *str,int error_value);//error handling function
/*this delay() function delay the further execution of program by specified milliseconds*/
void delay(int milliseconds){
    long pause;
    clock_t now,then;
    pause = milliseconds*(CLOCKS_PER_SEC/1000);
    now = then = clock();
    while( (now-then) < pause )
        now = clock();
}
/*clear_screen() clear entire screen and delete all lines saved in scrollback buffer and put the cursor to home position*/
void clear_screen(){
    write(1,"\x1b[2J",4);
    write(1,"\x1b[3J",4);
    write(1,"\x1b[H",3);
}    
/*offset_calculate() calculate the total number of characters above current row*/
int offset_calculate(int x){
    int offset = 0;
    for(int i = 0; i < x-1; i++)
        offset += row_col.col[i];
    return offset;
}
/* position_cursor() write the cursor to specified position on terminal window*/
void position_cursor(){
    char buf[10];
    int z = snprintf(buf , sizeof(buf),    "\x1b[%d;%dH", y,cx);
    write(1,buf,z);
}
/*initiate_screen() implement tildes symbol on terminal which  indicate the unwritten rows*/
void initiate_screen()
{
    if(ws.ws_row == 1 && ws.ws_col == 1)
        catch_error("initiate_screen",errno);
    else {
        static int k = 0;
        int c = row_col.row+1;
        if (k == 0){   
            write(1,"\x1b[2J",4);
            write(1,"\x1b[3J",4);
            k++;
            c = row_col.row;
        }
        char b[10];
        int z = snprintf(b , sizeof(b),    "\x1b[%d;%dH", c ,1);
        write(1,b,z);
        for(int i = c ; i <= ws.ws_row; i++){
            if (i == ws.ws_row){
                write(1,"~",1);
                break;
            }
            write(1,"~\n",2);
        }
    }
}
/*column_initializer() initializes the size of newly created column to 0*/
void column_initializer(){
    *(row_col.col+row_col.row-1) = 0;
}
/*memory for the number of columns is dynamically allocated using realloc*/
void allocate_column(){
    int *new = realloc(row_col.col,row_col.row*sizeof(int));
    if (new == NULL)
        catch_error("allocate_column",errno);
    row_col.col = new;
    column_initializer();
}
/*write_buffer_on_screen writes() the updated buffer named 'buf' on cleared screen after each keystroke*/
void write_buffer_on_screen(){
    clear_screen();
    write(STDOUT_FILENO,buff.str+offset_calculate(tindex),lenwbs);
    initiate_screen();
    position_cursor();
}
/*track_column() traces the number of columns in each row according to the current size of terminal window*/
void track_column(){
    ++(*(row_col.col+cy-1));
}
/*track_row_column() finds position of cursor according to current size of terminal window*/
void track_row_column(){
    cx=1,cy=1;
    row_col.col = (int*)malloc(1);
    row_col.row=1;
    allocate_column();
    for (int i = 0; i < buff.len; i++){   
        if (*(buff.str+i) == '\n' || (cx == ws.ws_col && *(buff.str+i) != '\0')){
            ++row_col.row;
            track_column();
            allocate_column();
            ++cy;
            cx = 1;
        }else{   
            ++cx;
            track_column();
        } 
    }
}
/*track_row_cloumn1() finds the postion of cursor on rewraping of terminal window*/
void track_row_column1(){
    int n = offset_calculate(cy)+cx;
    int m,j;
    track_row_column();
    for(int i = 1; i <= row_col.row; i++){
        m = offset_calculate(i);
        for (int j = 1; j <= row_col.col[i-1];j++){
            if(m+j == n ){
                cx1=j;
                cy1=i;
                break;
            }
        }
        if(m+j == n )
            break;
    }
    if(cy1 >= bindex){
        bindex = cy1+1;
        tindex = bindex - ws.ws_row;
        if(row_col.row +1 <= bindex ){ 
            bindex = row_col.row+1;
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
        }
        else lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    }
    else{
        bindex = tindex + ws.ws_row;
        if(row_col.row +1 <= bindex ){
            bindex = row_col.row+1;
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
        }
        else lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    }
    y = cy1-tindex+1;
}
/*delete_buffer() implements the delete key*/
void delete_buffer(){
    int offset = offset_calculate(cy1);
    memmove(buff.str+offset+cx1-1,buff.str+offset+cx1,buff.len-offset-cx1+1);
    --buff.len;
    int a = cx,c=cy;
    track_row_column();
    cx = a ,cy = c;
    if (row_col.row + 1 <= bindex || row_col.row < ws.ws_row){
        bindex = row_col.row+1;
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
    }else
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    write_buffer_on_screen();
}
/*append_buffer_r() appends characters at the end of buffer named 'buff' */
void append_buffer_r( char s , int len){
    char *new = realloc( buff.str, buff.len + len);
    if (new == NULL)
        return;
    memcpy((new+buff.len),&s,len);
    buff.str = new;
    buff.len += len;
}
/*sigwinchHandler() detects the change in the terminal size and carries out operation on respective change*/
static void sigwinchHandler(int sig){
    if (ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
        exit(EXIT_SUCCESS);
    track_row_column1();
    cx = cx1,cy = cy1;
    write_buffer_on_screen();
}
/*backspace_buffer() implements the backspace key on buffer named 'buff'*/
void backspace_buffer(){
    int offset = offset_calculate(cy1);
    memmove(buff.str+offset+cx1-2,buff.str+offset+cx1-1,buff.len-offset-cx1+1);
    --buff.len;
    char *new = realloc( buff.str, buff.len);
    buff.str = new;
    int a = cx,c=cy;
    track_row_column();
    cx = a ,cy = c;
    if (y == 1){
        tindex = cy;
        bindex = cy+ws.ws_row;
        if (row_col.row + 1 <= bindex || row_col.row < ws.ws_row){
            bindex = row_col.row+1;
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
        }else
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    }else if (row_col.row + 1 <= bindex){
        bindex = row_col.row+1;
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
    }else if (cy == row_col.row)
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
    else 
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    write_buffer_on_screen();
}
/*normalizeTerminal() enable raw input mode on terminal window,
Noncanonical mode, disable signals, extended input processing, and echoing
disable special handling of BREAK,
enable all output processing,
character at a time input with breaking*/
void normalizeTerminal(){
    if(tcgetattr(STDIN_FILENO, &termios_p) == -1)
        catch_error("tcgetattr",errno);
    termios_p1 = termios_p;
    p = termios_p;
    termios_p.c_lflag &=~(ICANON | ISIG | IEXTEN | ECHO);
    termios_p.c_iflag &=~(BRKINT | INPCK | ISTRIP | IXON );
    termios_p.c_cflag |= (CS8);
    termios_p.c_oflag |= (OPOST | ONLCR );
    termios_p.c_cc[VMIN] = 1; 
    termios_p.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH , &termios_p);
}
/*save file() saves the buffer named 'buff' in file. filename can be given as command line argument
or also can be specified at the time of saving*/
void save_file(char *argv[],bool EXIT_mode){
    clear_screen();
    denormalizeTerminal();
    char flag;
    FILE *fp;
    if (argv[1] == NULL){
        printf("%s","enter the file name:");
        scanf("%s",ch);
        while ((getchar()) != '\n'); 
        clear_screen();
        argv[1]=ch;
        fp = fopen(argv[1],"r");
        if( fp != NULL){   
            printf("%s","file already existed.want to replace it?(y/n):");
            scanf("%c",&flag);
            while ((getchar()) != '\n'); 
            fclose(fp);
            if (flag == 'y') {
                fp = fopen(argv[1],"w");
                fwrite(buff.str,1,buff.len,fp);
                fclose(fp);
            }
        }else {
            fp = fopen(argv[1],"w");
            fwrite(buff.str,1,buff.len,fp);
            fclose(fp);
        }
    }else{
        fp = fopen(argv[1],"w");
        if( fp == NULL)
            catch_error("save_file",errno);
        fwrite(buff.str,1,buff.len,fp);
        fclose(fp);
    }
    if (!EXIT_mode){
        delay(1000);
        clear_screen();
        initiate_screen();
        normalizeTerminal();
        write_buffer_on_screen();
    }
}
/*exit_terminal() exits the cli editor*/
void exit_terminal(char *argv[]){
    char flag;
    clear_screen();
    denormalizeTerminal();
    printf("%s","do you save changes on disk 'y/n':");
    scanf("%c",&flag);
    while ((getchar()) != '\n'); 
    if (flag == 'y')
        save_file(argv,true);
    if (buff.str != NULL)
        free(buff.str);
    if (row_col.col != NULL)
        free(row_col.col);
    exit(EXIT_SUCCESS);
}
/*append_buffer() appends new characters at specified position in buffer named 'buff'*/
void append_buffer( char s , int len){
    int offset = offset_calculate(cy1);
    if(offset+cx1 == buff.len+len)
        append_buffer_r(s,len);
    else{
        if (INS_mode && *(buff.str +offset+ cx1-1) != '\n')
            *(buff.str +offset+ cx1-1) = s;
        else{
            char *new = realloc( buff.str, buff.len + len);
            if (new == NULL)
                return;
            buff.str = new;
            buff.len += len;
            char *buffer1 = (char*)malloc(buff.len-offset-cx1);
            if (buffer1 == NULL)
                catch_error("append_buffer malloc",errno);
            memmove(buffer1,buff.str+offset+cx1-1,buff.len-offset-cx1);
            *(buff.str +offset+ cx1-1) = s;
            memmove(buff.str+offset+cx1,buffer1,buff.len-offset-cx1);
            free(buffer1);
        }
    }
    int a = cx,c=cy;
    track_row_column();
    cx = a ,cy = c;
    if(row_col.row <= ws.ws_row){
        tindex = 1,bindex = row_col.row+1;
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
    }else if (y == ws.ws_row){
        bindex = cy+1;
        tindex = bindex-ws.ws_row;
        if (cy == row_col.row)
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
        else
            lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;   
    }else 
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    write_buffer_on_screen();
}
/*get_windows_size() enquires current terminal window size and save it in  ws of type struct winsize*/
void get_windows_size(){ 
    if(ioctl(STDIN_FILENO,TIOCGWINSZ, &ws) == -1)
        catch_error("ioctl",errno);
}
/*enter_key() identifies the key pressed by user and return the corresponding value of key to calling function*/
int enter_key(char *argv[]){
    int nread;
    char c;
    char s[3];
    while((nread = read(STDIN_FILENO, &c, 1))== 0);
    if (nread == -1){
        if(errno == EINTR) c = NUL;
        else catch_error("enter_key", errno);
    }
    switch(c){
        case ESC:
            if (read(1,s,1)==0)
                return ESC;
            if (read(1,s+1,1) == 0)
                return ESC;
            if (s[0]=='O'){
                switch(s[1]){
                    case 'H':
                        return HOME;
                        break;
                    case 'F':
                        return END;
                        break;
                }
            }
            else if (s[0] == '['){
                switch(s[1]){
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
                    case 'H':
                        return HOME;
                        break;
                    case 'F':
                        return END;
                        break;
                }
                if(read(1,s+2,1) == 1 && s[2] =='~'){
                    switch(s[1]){
                        case '1':
                        case '7':
                            return HOME;
                            break;
                        case '2':
                            return INS;
                            break;
                        case '3':
                            return DEL;
                            break;
                        case '4':
                        case '8':
                            return END;
                            break;
                        case '5':
                            return PgUP;
                            break;
                        case '6':
                            return PgDn;
                            break;
                    }
                }
            }
        case  CTRL_KEY('q'):
            exit_terminal(argv);
            break;
        case CTRL_KEY('h'):
            return BACKSPACE;
            break;
        case CTRL_KEY('s'):
            return SAVE;
            break;
    }
    return c;
}
/* write_rows() process the key that are identified by enter_key(). if any pre existing file is opened then
firstly the content of that file is written on terminal window and then the keys are processed as identified
by enter_key() */
void write_rows(char *argv[]){   
    cx = 1,cy = 1;
    tindex = 1,bindex = ws.ws_row+1;
    lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
    if(row_col.row <= ws.ws_row){
        tindex = 1,bindex = row_col.row+1;
        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
    }
    write(STDOUT_FILENO,"\x1b[H",3);
    write(STDOUT_FILENO,buff.str,lenwbs);
    write(STDOUT_FILENO,"\x1b[H",3);
    while(1){
        int c = enter_key(argv);
        switch(c){
            case BACKSPACE:
                if (cx == 1 && cy == 1)
                    break;
                if (cx == 1 && cy > 1){
                    cx1= cx;
                    cy1 = cy;
                    --cy;
                    if (y != 1)
                        --y;
                    cx = row_col.col[cy-1];
                }else{
                    cx1= cx;
                    cy1 = cy;
                    --cx;
                }
                backspace_buffer();    
                break;
            case DEL:
                cx1= cx;
                cy1 = cy;
                int offset = offset_calculate(cy1);
                if(offset+cx1>buff.len)
                    break;
                delete_buffer();
                break;
            case ARROW_DOWN:
                if(cy < row_col.row){
                    ++cy;
                    y = (y == ws.ws_row)?ws.ws_row:y+1;
                    if (y == ws.ws_row){
                        bindex = cy+1;
                        tindex = bindex-ws.ws_row;
                        if (cy == row_col.row)
                            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
                        else
                            lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
                        
                    }
                    if (cx > *(row_col.col+cy-1)){
                        if (cy == row_col.row)
                            cx = *(row_col.col+cy-1)+1;
                        else
                            cx = (*(row_col.col+cy-1)>0)?(*(row_col.col+cy-1)):1;
                        write_buffer_on_screen();
                    }else
                        write_buffer_on_screen();
                }
                break;
            case ARROW_UP:
                if (cy > 1){
                    --cy;
                    y = (y == 1)?1:y-1;
                    if (y == 1){
                        tindex = cy;
                        bindex = cy+ws.ws_row;
                        if (row_col.row <= bindex-1){   
                            bindex = row_col.row+1;
                            lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
                        }else            
                            lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
                    }
                    if (cx > *(row_col.col+cy-1)){
                        cx = (*(row_col.col+cy-1)>0)?(*(row_col.col+cy-1)):1;
                        write_buffer_on_screen();
                    }else
                        write_buffer_on_screen();
                }
                break;
            case ARROW_LEFT:
                if (cx > 1){
                    --cx;
                    position_cursor();
                }
                break;
            case ARROW_RIGHT:
                if (cy == row_col.row){
                    if (cx < *(row_col.col+cy-1)+1){
                        ++cx;
                        position_cursor();
                    }
                }else if (cx < *(row_col.col+cy-1)){
                    ++cx;
                    position_cursor();
                }
                break;
            case ENTERKEY:
                cx1 = cx;
                cy1 = cy;
                char str = '\n';
                if (row_col.row == cy){
                    track_column();
                    ++cy;
                    ++row_col.row;
                    allocate_column();
                }else{
                    ++row_col.row;
                    allocate_column();
                    ++cy;
                    track_column();
                }
                if (y < ws.ws_row)
                    ++y;
                cx = 1;
                append_buffer(str,1);
                break;
            case TAB:
                if(row_col.col[cy-1]+4 < ws.ws_col)
                    for(int i = 0; i <= 3; i++){
                        cy1=cy;
                        cx1 = cx;
                        ++cx;
                        append_buffer(' ',1);
                    }
                break;
            case HOME:
                cx = 1;
                position_cursor();
                break;
            case END:
                cx = (row_col.row == cy)?(row_col.col[cy-1]+1):row_col.col[cy-1];
                position_cursor();
                break;
            case PgUP:
                if (tindex > 1 && row_col.row > ws.ws_row ){
                    tindex = tindex - ws.ws_row;
                    if (tindex >= 1){
                        bindex = tindex+ws.ws_row;
                        cy -= ws.ws_row;
                    }else{
                        tindex = 1;
                        bindex = tindex+ws.ws_row;
                        cy = y = tindex;
                    }
                    cx = (cx > row_col.col[cy-1])?1:cx;
                    lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
                    write_buffer_on_screen();
                }
                break;
            case PgDn:
                if (bindex <= row_col.row && row_col.row > ws.ws_row){
                    tindex = bindex;
                    bindex = tindex+ws.ws_row;
                    if (row_col.row+1 <= bindex ){
                        bindex = row_col.row+1;
                        lenwbs = offset_calculate(bindex)-offset_calculate(tindex);
                        if (cy+ws.ws_row > bindex ){
                            cy = tindex;
                            y = 1;
                        }else 
                            cy = cy+ws.ws_row; 
                    }else{
                        cy = cy+ws.ws_row; 
                        lenwbs = offset_calculate(bindex)-offset_calculate(tindex)-1;
                    }
                    cx = (cx > row_col.col[cy-1])?1:cx;
                    write_buffer_on_screen();
                }
                break;
            case INS:
                if (INS_mode == false)
                    INS_mode = true;
                else 
                    INS_mode = false;
                break;
            case SAVE:
                save_file(argv,false);
                break;
            default:
                if(isprint(c)){
                if (cx == ws.ws_col){
                    cx1=cx;
                    cy1=cy;
                    cx = 1;
                    ++cy;
                    if (y < ws.ws_row)
                       ++y;
                }else if(cx < ws.ws_col){    
                    cx1 = cx;
                    cy1 = cy;
                    ++cx;
                }
                append_buffer(c,1);
                }
        }
    }
}
/*buffer_to_window() firstly reads the existing file if opened and then copy the file element to buffer named 'buff'*/
void buffer_to_window( char *argv[]){
    char ch;
    char str[2];
    FILE *fp;
    if (argv[1] != NULL){
        if ((fp = fopen(argv[1],"r")) == NULL)
            return;
        else{
            while((ch = fgetc(fp))!= EOF){
                if (ch == '\t'){
                    for(int i = 0; i <= 3; i++){
                        append_buffer_r(' ',1);
                        ++cx;
                        track_column();
                        if (cx-1 == ws.ws_col ){
                            ++row_col.row;
                            track_column();
                            allocate_column();
                            ++cy;
                            cx = 1;
                        }
                    }
                }else if (ch == '\n' || (cx == ws.ws_col && ch != '\n')){
                    append_buffer_r(ch,1);
                    ++row_col.row;
                    track_column();
                    allocate_column();
                    ++cy;
                    cx = 1;
                }else{
                    append_buffer_r(ch,1);
                    ++cx;
                    track_column();
                } 
            }
            fclose(fp);
        }
    }else
        return;
}
/*editor_write() is called  by main() after raw input mode is enabled*/
void editor_write( char *argv[]){
    get_windows_size();
    buffer_to_window(argv);
    initiate_screen();
    write_rows(argv);
}
int main(int argc , char *argv[]){    
    if(isatty(0) && isatty(1) && isatty(2)){//check whether the terminal is unix like or not
        struct sigaction sa;
        allocate_column();
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = sigwinchHandler;
        if (sigaction(SIGWINCH, &sa, NULL) == -1)
            catch_error("sigaction",errno);
        normalizeTerminal();
        editor_write(argv);
    }else
        printf("%s\n","file descripter must be associated with unix-like terminal");
    return 0;
}
/*denormalizeTerminal() disables raw input handling mode*/
void denormalizeTerminal(){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_p1) == -1)
        catch_error("tcsetattr",errno);
}
/*catch_error() is error handling function*/
void catch_error( char *str,int error_value){
    clear_screen();
    printf("%s:%s\n",str,strerror(error_value));
    if (buff.str != NULL)
        free(buff.str);
    if (row_col.col != NULL)
        free(row_col.col);
    denormalizeTerminal();
    exit(EXIT_FAILURE);
}