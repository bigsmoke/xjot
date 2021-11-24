#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define XJOT_TO_XML_BUFFER_SIZE_IN_BYTES 1024


// This code follows Nubaria's guidelines for working with UTF-8:
// http://www.nubaria.com/en/blog/?p=289

struct
{
} xjot_attr;

typedef enum xjot_node_type
{
    XjotRootNode,
    XjotElement,
    XjotComment,
    XjotProcessingInstruction,
    XjotText,
    XjotMaybeElementOrMaybeProcessingInstruction
} xjot_node_type;

typedef struct xjot_node
{
    struct xjot_node* parent_node;
    unsigned int node_depth;
    enum xjot_node_type node_type;
    char* node_name;
    unsigned int start_line;
    unsigned int start_column;
} xjot_node;

xjot_node* _xjot_new_child_node(
        xjot_node* parent,
        xjot_node_type node_type,
        unsigned int start_line,
        unsigned int start_column)
{
    xjot_node* child_node = malloc(sizeof(xjot_node));
    printf("Post-malloc\n");
    (*child_node).parent_node = parent;
    (*child_node).node_depth = (*parent).node_depth + 1;
    (*child_node).node_type = node_type;
    (*child_node).start_line = start_line;
    (*child_node).start_column = start_column;

    return child_node;
}

xjot_node* _xjot_exit_child_node(xjot_node* child)
{
    xjot_node* parent = (*child).parent_node;
    if ((*child).node_name != NULL)
    {
        free((*child).node_name);
    }
    free(child);

    return parent;
}

void _xjot_to_xml_with_buf(int in_fd, int out_fd, void* buf, size_t buf_size)
{
    int n;
    int c;
    int line_no = 1;
    int col_no = 0;
    char byte;

    xjot_node root = {
        .parent_node = NULL,
        .node_depth = 0,
        .node_type = XjotRootNode,
        .node_name = NULL,
        .start_line = 1,
        .start_column = 1,
    };
    xjot_node* context = &root;

    while ((n = read(in_fd, &buf, buf_size)) != 0)
    {
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
            }
            fputs(strerror(errno), stderr);
            break;
        }

        printf("a\n");
        printf("b\n");

        // Loop through characters within buffer:
        for (c = 0; c < n; c++)
        {
            col_no++;

            // FIXME: Why do I get a segmentation fault as soon as I try to access a single byte in `buf`?
            byte = ((char*)buf)[0];

            switch (byte)
            {
                // TODO: \n\r support
                case '\n':
                    line_no++;
                    col_no = 1;
                    break;
                case '<':
                    printf("Child\n");
                    context = _xjot_new_child_node(
                            context, XjotMaybeElementOrMaybeProcessingInstruction, line_no, col_no
                    );
                    break;
                case '>':
                    context = _xjot_exit_child_node(context);
                    break;
            }
        }
    }
}

/**
 * Feed the xjot input found in `in_fd` as regular XML into `out_fd`.
 *
 * both `in_fd` and `out_fd` must be POSIX file descriptors.
 */
void xjot_to_xml(int in_fd, int out_fd)
{
    char buffer[XJOT_TO_XML_BUFFER_SIZE_IN_BYTES] = {'\0'};
    _xjot_to_xml_with_buf(in_fd, out_fd, &buffer, XJOT_TO_XML_BUFFER_SIZE_IN_BYTES);
}


int main(int argc, char** argv)
{
    xjot_to_xml(STDIN_FILENO, STDOUT_FILENO);

    return 0;
}


// vim: set expandtab tabstop=4 shiftwidth=4:
