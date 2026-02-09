# OS---PA-1
first project assignment of operating systems class

Name: Evan McCann

Design of wc_mul.c:
    Mult-process Structure:
        wc_mul.c uses multi-process and the parent process divides the input file into equal chunks for each child process. The number of children are based on the amount given in the input. It assigns each chunk by getting the total size (fseek/ftell) and then dividing it by number of children.

    IPC:
        child:
            Each child has its own pipe and is forked. It closes the read end and then opens the file and calls word_count with its given offset and size. It then writes count_t to write end of the pipe and then exits. The last child gets all the remainder bytes, like if there is an odd number so the whole file goes through the code properly.
        parent:
            The parent closes all write ends after forking the children, opposite of children. It calls waitpid for each child, checks exit status, reads count_t and then takes all the line, word, and character counts and prints the final total
            
Handling crashes:
    Successful children:
        Parent waits with waitpid and if children successful their results are sent to parent.
    Failed children:
        I used an internal loop in the parent where, if a child fails, it is immediately retried.
        I did this because, if a child fails, retrying immedaitely versuses waiting did not seem like a big deal.
        Also immediate seemed much easier to code.
        I thought about it similar to RTT and packets going missing from Computer Networks.
        I figured each child was its own packet and if one fails I need to go back as soon as possible so why not do it immediately.
