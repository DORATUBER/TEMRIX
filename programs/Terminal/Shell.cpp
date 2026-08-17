#include "Shell.hpp"
#include "TextEditor.hpp"

namespace
{
uint32_t spawnTRXViaFs(FsClient &client, const char *path, int argc, const char *const argv[])
{
    uint64_t virt = 0;
    uint32_t len = 0;

    if (!client.readFile(path, &virt, &len))
        return (uint32_t)-1;

    return spawnFromBufferWithArgs(path, reinterpret_cast<uint8_t *>(virt), len,
                                    argc, argv,
                                    /*requestedCapabilities*/ 0,
                                    /*requestDeviceGrant*/ false,
                                    /*deviceGrantKind*/ 0,
                                    /*deviceGrantParam*/ 0);
}

bool StrEq(const char *a, const char *b)
{
    uint32_t i = 0;
    while (a[i] && b[i])
    {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

int Tokenize(char *line, char **argv, int maxArgs)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < maxArgs)
    {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p++ = '\0'; }
    }
    return argc;
}

void IntToStr(uint32_t v, char *out)
{
    char tmp[16];
    int n = 0;
    if (v == 0) { out[0] = '0'; out[1] = '\0'; return; }
    while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}

void JoinPath(const char *cwd, const char *arg, char *out, uint32_t outSize)
{
    const char *parts[64];
    uint32_t partLen[64];
    int depth = 0;

    auto pushComponent = [&](const char *start, uint32_t len)
    {
        if (len == 0) return;
        if (len == 1 && start[0] == '.') return; 
        if (len == 2 && start[0] == '.' && start[1] == '.')
        {
            if (depth > 0) depth--; 
            return;
        }
        if (depth < 64)
        {
            parts[depth] = start;
            partLen[depth] = len;
            depth++;
        }
    };

    auto walk = [&](const char *s)
    {
        uint32_t i = 0;
        while (s[i])
        {
            while (s[i] == '/') i++;
            uint32_t start = i;
            while (s[i] && s[i] != '/') i++;
            pushComponent(s + start, i - start);
        }
    };

    if (arg[0] != '/')
        walk(cwd); 

    walk(arg);

    uint32_t o = 0;
    if (depth == 0)
    {
        if (outSize > 1) { out[0] = '/'; out[1] = '\0'; }
        return;
    }
    for (int d = 0; d < depth; d++)
    {
        if (o < outSize - 1) out[o++] = '/';
        for (uint32_t k = 0; k < partLen[d] && o < outSize - 1; k++)
            out[o++] = parts[d][k];
    }
    out[o] = '\0';
}
} 

void RunCommand(FsClient &fs, char *cwd, const char *cmdIn, TermGrid &grid, bool &fullRedraw, Window &term, KeyEdgeTracker &keyTracker)
{
    char line[256];
    {
        uint32_t i = 0;
        while (cmdIn[i] && i < 255) { line[i] = cmdIn[i]; i++; }
        line[i] = '\0';
    }

    char *argv[16];
    int argc = Tokenize(line, argv, 16);
    if (argc == 0) return;

    const char *cmd = argv[0];
    char pathBuf[512];

    if (StrEq(cmd, "help"))
    {
        grid.Print("commands: ls cd pwd cat run edit mkdir touch rm write cp stat echo clear help\n");
        grid.Print("(mkdir/touch/rm/write/cp/edit work on the TemrixFs volume; ext4 stays read-only)\n");
        grid.Print("edit <path>: nano-style editor. arrows/home/end/pgup/pgdn to move, ^S save, ^X exit\n");
    }
    else if (StrEq(cmd, "pwd"))
    {
        grid.Print(cwd);
        grid.Print("\n");
    }
    else if (StrEq(cmd, "clear"))
    {
        grid.Init(grid.cols * CHAR_W, grid.rows * CHAR_H);
        fullRedraw = true;
    }
    else if (StrEq(cmd, "echo"))
    {
        for (int i = 1; i < argc; i++)
        {
            grid.Print(argv[i]);
            if (i + 1 < argc) grid.Print(" ");
        }
        grid.Print("\n");
    }
    else if (StrEq(cmd, "cd"))
    {
        if (argc < 2)
        {
            cwd[0] = '/'; cwd[1] = '\0';
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));

            uint32_t size = 0;
            if (!fs.statFile(pathBuf, &size))
            {
                grid.Print("cd: no such path: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else
            {
                uint32_t i = 0;
                while (pathBuf[i] && i < 255) { cwd[i] = pathBuf[i]; i++; }
                cwd[i] = '\0';
            }
        }
    }
    else if (StrEq(cmd, "ls"))
    {
        const char *target = argc >= 2 ? argv[1] : cwd;
        JoinPath(cwd, target, pathBuf, sizeof(pathBuf));

        static constexpr uint32_t MAX_LIST = 128;
        FsDirEntry entries[MAX_LIST];
        uint32_t count = 0;

        if (!fs.listDirectory(pathBuf, entries, MAX_LIST, &count) || count == 0)
        {
            grid.Print("(empty or not a directory)\n");
        }
        else
        {
            for (uint32_t i = 0; i < count; i++)
            {
                grid.Print(entries[i].name);
                if (entries[i].fileType == FsTypeDirectory) grid.Print("/");
                grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "cat"))
    {
        if (argc < 2)
        {
            grid.Print("usage: cat <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));

            uint64_t virt = 0;
            uint32_t len = 0;
            if (!fs.readFile(pathBuf, &virt, &len))
            {
                grid.Print("cat: no such file: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else
            {
                const char *data = reinterpret_cast<const char *>(virt);
                for (uint32_t i = 0; i < len; i++)
                    grid.Put(data[i]);
                if (len == 0 || data[len - 1] != '\n') grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "run"))
    {
        if (argc < 2)
        {
            grid.Print("usage: run <path> [args...]\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));

            const char *childArgv[16];
            int childArgc = 0;
            childArgv[childArgc++] = pathBuf;
            for (int i = 2; i < argc && childArgc < 16; i++)
                childArgv[childArgc++] = argv[i];

            constexpr int STRESS_ITERS = 100;
            int failCount = 0;

            for (int iter = 0; iter < STRESS_ITERS; iter++)
            {
                grid.Print("run: iter=");
                char itbuf[12];
                grid.Print(String::FromU64((uint64_t)iter, itbuf));
                grid.Print(" spawning ");
                grid.Print(pathBuf);
                grid.Print("\n");

                uint32_t pid = spawnTRXViaFs(fs, pathBuf, childArgc, childArgv);

                if (pid == (uint32_t)-1)
                {
                    failCount++;
                    grid.Print("run: FAILED at iter=");
                    grid.Print(String::FromU64((uint64_t)iter, itbuf));
                    grid.Print(" path=");
                    grid.Print(pathBuf);
                    grid.Print("\n");
                }
                else
                {
                    grid.Print("run: iter=");
                    grid.Print(String::FromU64((uint64_t)iter, itbuf));
                    grid.Print(" ok pid=");
                    grid.Print(String::FromU64((uint64_t)pid, itbuf));
                    grid.Print("\n");
                }
            }

            grid.Print("run: stress complete, failures=");
            char failbuf[12];
            grid.Print(String::FromU64((uint64_t)failCount, failbuf));
            grid.Print("/");
            grid.Print(String::FromU64((uint64_t)STRESS_ITERS, failbuf));
            grid.Print("\n");
        }
    }
    else if (StrEq(cmd, "edit"))
    {
        if (argc < 2)
        {
            grid.Print("usage: edit <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));
            RunTextEditor(fs, term, pathBuf, keyTracker);
            
            
            fullRedraw = true;
        }
    }
    else if (StrEq(cmd, "mkdir"))
    {
        if (argc < 2)
        {
            grid.Print("usage: mkdir <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));
            FsStatus st = fs.createDirectory(pathBuf);
            if (st == FsNotSupported)
            {
                grid.Print("mkdir: read-only mount: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else if (st != FsDone)
            {
                grid.Print("mkdir: failed (exists or bad path): ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "touch"))
    {
        if (argc < 2)
        {
            grid.Print("usage: touch <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));
            FsStatus st = fs.createFile(pathBuf);
            if (st == FsNotSupported)
            {
                grid.Print("touch: read-only mount: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else if (st != FsDone)
            {
                grid.Print("touch: failed (exists or bad path): ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "rm"))
    {
        if (argc < 2)
        {
            grid.Print("usage: rm <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));
            FsStatus st = fs.removeFile(pathBuf);
            if (st == FsNotSupported)
            {
                grid.Print("rm: read-only mount: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else if (st != FsDone)
            {
                grid.Print("rm: failed (non-empty dir or not found): ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "write"))
    {
        if (argc < 2)
        {
            grid.Print("usage: write <path> <text...>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));

            char text[256];
            uint32_t len = 0;
            for (int i = 2; i < argc && len < sizeof(text) - 1; i++)
            {
                const char *a = argv[i];
                while (*a && len < sizeof(text) - 1)
                    text[len++] = *a++;
                if (i + 1 < argc && len < sizeof(text) - 1)
                    text[len++] = ' ';
            }
            text[len] = '\0';

            fs.truncateFile(pathBuf, 0); 
            FsStatus st = fs.writeFile(pathBuf, 0, (const uint8_t *)text, len);
            if (st == FsNotSupported)
            {
                grid.Print("write: read-only mount: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else if (st != FsDone)
            {
                grid.Print("write: failed (not found or bad path): ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
        }
    }
    else if (StrEq(cmd, "cp"))
    {
        if (argc < 3)
        {
            grid.Print("usage: cp <src> <dst>\n");
        }
        else
        {
            char srcPath[512], dstPath[512];
            JoinPath(cwd, argv[1], srcPath, sizeof(srcPath));
            JoinPath(cwd, argv[2], dstPath, sizeof(dstPath));

            uint64_t virt = 0;
            uint32_t len = 0;
            if (!fs.readFile(srcPath, &virt, &len))
            {
                grid.Print("cp: no such file: ");
                grid.Print(srcPath);
                grid.Print("\n");
            }
            else
            {
                fs.createFile(dstPath); 
                fs.truncateFile(dstPath, 0);
                FsStatus st = fs.writeFile(dstPath, 0, reinterpret_cast<const uint8_t *>(virt), len);
                if (st == FsNotSupported)
                {
                    grid.Print("cp: read-only mount: ");
                    grid.Print(dstPath);
                    grid.Print("\n");
                }
                else if (st != FsDone)
                {
                    grid.Print("cp: failed to write: ");
                    grid.Print(dstPath);
                    grid.Print("\n");
                }
            }
        }
    }
    else if (StrEq(cmd, "stat"))
    {
        if (argc < 2)
        {
            grid.Print("usage: stat <path>\n");
        }
        else
        {
            JoinPath(cwd, argv[1], pathBuf, sizeof(pathBuf));
            uint32_t size = 0;
            if (!fs.statFile(pathBuf, &size))
            {
                grid.Print("stat: no such path: ");
                grid.Print(pathBuf);
                grid.Print("\n");
            }
            else
            {
                grid.Print(pathBuf);
                grid.Print(": ");

                char numBuf[16];
                IntToStr(size, numBuf); 
                grid.Print(numBuf);
                grid.Print(" bytes\n");
            }
        }
    }
    else
    {
        grid.Print(cmd);
        grid.Print(": command not found (try 'help')\n");
    }
}