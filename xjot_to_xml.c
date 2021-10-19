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

/**
 * Not every `xjot_node_type` corresponds to a precise XML node type.
 * When encountering a '<', for example, the state is `XjotPointyOpening`,
 * because the stream reader cannot determine yet at that point whether it
 * will be an element, a processing instruction or a comment that is being
 * started at that position in the input.
 */
typedef enum xjot_node_type
{
    XjotRootNode,
    XjotPointyOpening,
    XjotElementName,
    XjotElementAttributes,
    XjotElementAttributeName,
    XjotElementAttributeValue,
    XjotElementContent,
    XjotElementInline,
    XjotElementMultiline,
    XjotComment,
    XjotCommentInline,
    XjotCommentMultiline,
    XjotProcessingInstruction,
    XjotText,
} xjot_node_type;

typedef struct xjot_node
{
    struct xjot_node* parent_node;
    unsigned int node_depth;
    enum xjot_node_type node_type;
    char* node_name;
    char* attr_name;
    char attr_start_quote;
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

char* _xjot_append_char_to_string(char* str, char c)
{
    char* extended_string = (str == NULL ? calloc(2, sizeof(char)) : realloc(str, sizeof(char) * (strlen(str) + 1)));
    return strncat(extended_string, &c, 1);
}

void _xjot_to_xml_with_buf(int in_fd, int out_fd, void* buf, size_t buf_size)
{
    int line_no = 1;
    int col_no = 0;
    xjot_node root = {
        .parent_node = NULL,
        .node_depth = 0,
        .node_type = XjotRootNode,
        .node_name = NULL,
        .attr_name = NULL,
        .attr_start_quote = '\0',
        .start_line = 1,
        .start_column = 1,
    };
    xjot_node* context = &root;

    int n;
    while ((n = read(in_fd, buf, buf_size)) != 0)
    {
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN)
            {
            }
            fputs(strerror(errno), stderr);
            break;
        }

        char previous_byte = '\0';
        // Loop through characters within buffer:
        for (int c = 0; c < n; c++)
        {
            col_no++;

            char byte = ((char*)buf)[c];

            switch (byte)
            {
                case '\n':
                case '\r':
                    if (byte != previous_byte || (previous_byte != '\n' && previous_byte != '\r'))
                        line_no++;
                    col_no = 1;
                    if ((*context).node_type != XjotElementContent)
                    {
                        putchar(byte);
                    }
                    if ((*context).node_type == XjotElementName)
                    {
                        (*context).node_type = XjotElementAttributes;
                    }
                    break;
                case '<':
                    context = _xjot_new_child_node(context, XjotPointyOpening, line_no, col_no);
                    break;
                case '>':
                    if ((*context).node_type == XjotElementContent)
                    {
                        printf("</%s>", (*context).node_name);
                    }
                    else if ((*context).node_type == XjotProcessingInstruction)
                    {
                        printf("?>");
                    }
                    context = _xjot_exit_child_node(context);
                    break;
                case '/':
                    if ((*context).node_type == XjotElementName)
                    {
                        (*context).node_type == XjotElementContent;
                        printf("<%s>", (*context).node_name);
                    }
                    break;
                case '@':
                    if ((*context).node_type == XjotElementAttributes)
                    {
                        (*context).node_type == XjotElementAttributeName;
                    }
                    break;
                case '=':
                    if ((*context).node_type == XjotElementAttributeName)
                    {
                        (*context).node_type == XjotElementAttributeValue;
                        printf("%s=", (*context).attr_name);
                    }
                    break;
                case '\'':
                    if ((*context).node_type == XjotElementAttributeValue)
                    {

                    }
                    break;
                case '?':
                    if ((*context).node_type == XjotPointyOpening)
                    {
                        (*context).node_type = XjotProcessingInstruction;
                        printf("<?");
                    }
                    break;
                default:
                    if ((*context).node_type == XjotPointyOpening)
                    {
                        (*context).node_type = XjotElementName;
                    }

                    if ((*context).node_type == XjotElementName)
                    {
                        (*context).node_name = _xjot_append_char_to_string((*context).node_name, byte);
                    }
                    else if ((*context).node_type == XjotElementAttributeName)
                    {
                        (*context).attr_name = _xjot_append_char_to_string((*context).attr_name, byte);
                    }
                    else if ((*context).node_type == XjotElementContent)
                    {
                        putchar(byte);
                    }
                    else if ((*context).node_type == XjotProcessingInstruction)
                    {
                        putchar(byte);
                    }
                    else if ((*context).node_type == XjotProcessingInstruction)
                    {
                    }
                    break;
            }

            previous_byte = byte;
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
