#include <clang-c/Index.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    PTR_UNKNOWN = 0,
    PTR_MALLOCED,
    PTR_FREED
} PtrState;

#define MAX_VARS 100

typedef struct {
    char name[128];
    PtrState state;
} VarInfo;

typedef struct {
    VarInfo varTable[MAX_VARS];
    int varCount;
    char currentFunction[128];
} AnalysisContext;

static int isFunctionCall(CXCursor cursor, const char *funcName) {
    if (clang_getCursorKind(cursor) != CXCursor_CallExpr) return 0;
    CXString fn = clang_getCursorSpelling(cursor);
    const char *cfn = clang_getCString(fn);
    int result = (cfn && (strcmp(cfn, funcName) == 0));
    clang_disposeString(fn);
    return result;
}

// func: getVarNameFromArg
static const char* getVarNameFromArg(CXCursor argCursor, char *outName, size_t outSize) {
    if (clang_getCursorKind(argCursor) == CXCursor_DeclRefExpr) {
        CXString name = clang_getCursorSpelling(argCursor);
        const char *cName = clang_getCString(name);
        if (cName) {
            strncpy(outName, cName, outSize - 1);
            outName[outSize - 1] = '\0';
        }
        clang_disposeString(name);
        return outName;
    }
    return NULL;
}

// func: findVarIndex
static int findVarIndex(AnalysisContext *ctx, const char* varName) {
    for(int i = 0; i < ctx->varCount; i++) {
        if(strcmp(ctx->varTable[i].name, varName) == 0) {
            return i;
        }
    }
    return -1;
}

// func: addVar
static void addVar(AnalysisContext *ctx, const char* varName) {
    if(ctx->varCount < MAX_VARS) {
        strncpy(ctx->varTable[ctx->varCount].name, varName, 127);
        ctx->varTable[ctx->varCount].name[127] = '\0';
        ctx->varTable[ctx->varCount].state = PTR_UNKNOWN;
        ctx->varCount++;
    }
}

// func: recordMalloc
static void recordMalloc(AnalysisContext *ctx, const char* varName) {
    int idx = findVarIndex(ctx, varName);
    if(idx < 0) {
        addVar(ctx, varName);
        idx = ctx->varCount - 1;
    }
    ctx->varTable[idx].state = PTR_MALLOCED;
}

// func: recordFree
static void recordFree(AnalysisContext *ctx, const char* varName) {
    int idx = findVarIndex(ctx, varName);
    if(idx >= 0) {
        if(ctx->varTable[idx].state == PTR_MALLOCED) {
            ctx->varTable[idx].state = PTR_FREED;
        } else if(ctx->varTable[idx].state == PTR_FREED) {
            printf("[WARNING] Double free detected on variable '%s' in function '%s'\n",
                varName, ctx->currentFunction);
        }
    }
}

// func: checkLeakAtFunctionEnd
static void checkLeakAtFunctionEnd(AnalysisContext *ctx) {
    for(int i = 0; i < ctx->varCount; i++) {
        if(ctx->varTable[i].state == PTR_MALLOCED) {
            printf("[WARNING] Potential memory leak: variable '%s' in function '%s' not freed.\n",
                ctx->varTable[i].name, ctx->currentFunction);
        }
    }
}

// func: extended check for strcpy
static int checkStrcpyExtended(CXCursor callExpr) {
    int numArgs = clang_Cursor_getNumArguments(callExpr);
    if(numArgs < 2) return 0; 
    // Arg0 = dst, Arg1 = src
    CXCursor arg1 = clang_Cursor_getArgument(callExpr, 1);
    if (clang_getCursorKind(arg1) == CXCursor_StringLiteral) {
        CXString str = clang_getCursorSpelling(arg1);
        const char *lit = clang_getCString(str);
        size_t len = strlen(lit);
        clang_disposeString(str);
        // Example: if literal < 32 => consider safe
        if(len < 32) {
            return 1; 
        }
    }
    return 0;
}

static const char* DANGEROUS_FUNCS[] = {
    "strcpy", "strcat", "sprintf", "gets", "scanf", NULL
};

// func: checkDangerousCall
static int checkDangerousCall(CXCursor callExpr) {
    CXString fnName = clang_getCursorSpelling(callExpr);
    const char *callName = clang_getCString(fnName);
    int isSafe = 1;
    for(int i=0; DANGEROUS_FUNCS[i] != NULL; i++) {
        if(strcmp(callName, DANGEROUS_FUNCS[i]) == 0) {
            isSafe = 0;
            if(strcmp(callName, "strcpy") == 0) {
                // apply extended check
                if(checkStrcpyExtended(callExpr)) {
                    isSafe = 1;
                }
            }
            break;
        }
    }
    clang_disposeString(fnName);
    return isSafe;
}

// func: checkPrintfFormatBug
static int checkPrintfFormatBug(CXCursor callExpr) {
    if(!isFunctionCall(callExpr, "printf")) return 1;
    int numArgs = clang_Cursor_getNumArguments(callExpr);
    if(numArgs == 1) {
        return 0;
    }
    return 1;
}

// func: visitorCallExpr
static enum CXChildVisitResult visitorCallExpr(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    AnalysisContext *ctx = (AnalysisContext*)client_data;
    if(clang_getCursorKind(cursor) == CXCursor_CallExpr) {
        if(isFunctionCall(cursor, "malloc") || isFunctionCall(cursor, "calloc")) {
            // naive approach: no real tracking "var = malloc"
        } else if(isFunctionCall(cursor, "free")) {
            unsigned int argCount = clang_Cursor_getNumArguments(cursor);
            if(argCount > 0) {
                CXCursor arg0 = clang_Cursor_getArgument(cursor, 0);
                char varName[128];
                if(getVarNameFromArg(arg0, varName, sizeof(varName))) {
                    recordFree(ctx, varName);
                }
            }
        }
        int safeCall = checkDangerousCall(cursor);
        if(!safeCall) {
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file;
            unsigned line, col;
            clang_getSpellingLocation(loc, &file, &line, &col, NULL);
            CXString fileName = clang_getFileName(file);
            const char *cFileName = clang_getCString(fileName);
            CXString fn = clang_getCursorSpelling(cursor);
            const char *cfn = clang_getCString(fn);
            printf("[WARNING] Potentially unsafe function '%s' at %s:%u:%u\n",
                cfn, cFileName ? cFileName : "?", line, col);
            clang_disposeString(fn);
            clang_disposeString(fileName);
        }
        int safePrintf = checkPrintfFormatBug(cursor);
        if(!safePrintf) {
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file;
            unsigned line, col;
            clang_getSpellingLocation(loc, &file, &line, &col, NULL);
            CXString fileName = clang_getFileName(file);
            const char *cFileName = clang_getCString(fileName);
            printf("[WARNING] Format string bug with 'printf' at %s:%u:%u\n",
                cFileName ? cFileName : "?", line, col);
            clang_disposeString(fileName);
        }
    }
    return CXChildVisit_Recurse;
}

// func: visitorMain
static enum CXChildVisitResult visitorMain(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    AnalysisContext *ctx = (AnalysisContext*)client_data;
    enum CXCursorKind kind = clang_getCursorKind(cursor);
    if(kind == CXCursor_FunctionDecl) {
        CXString fn = clang_getCursorSpelling(cursor);
        const char* cfn = clang_getCString(fn);
        strncpy(ctx->currentFunction, cfn ? cfn : "???", 127);
        ctx->currentFunction[127] = '\0';
        clang_disposeString(fn);
        ctx->varCount = 0;
        clang_visitChildren(cursor, visitorCallExpr, ctx);
        checkLeakAtFunctionEnd(ctx);
    }
    return CXChildVisit_Recurse;
}

// func: main
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source-file.c> [<source-file-2.c> ...]\n", argv[0]);
        return 1;
    }
    CXIndex index = clang_createIndex(0, 0);
    for(int i = 1; i < argc; i++) {
        const char* filename = argv[i];
        printf("\n=== Analyzing: %s ===\n", filename);
        CXTranslationUnit tu = clang_parseTranslationUnit(
            index,
            filename,
            NULL, 0,
            NULL, 0,
            CXTranslationUnit_None
        );
        if(!tu) {
            fprintf(stderr, "Cannot parse %s\n", filename);
            continue;
        }
        AnalysisContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
        clang_visitChildren(rootCursor, visitorMain, &ctx);
        clang_disposeTranslationUnit(tu);
    }
    clang_disposeIndex(index);
    return 0;
}
