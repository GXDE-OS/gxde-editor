#include <cstdlib>

int runEditorLanguageTest(int argc, char *argv[]);
int runSyntaxUtilsTest(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    int status = EXIT_SUCCESS;

    status |= runSyntaxUtilsTest(argc, argv);
    status |= runEditorLanguageTest(argc, argv);

    return status;
}
