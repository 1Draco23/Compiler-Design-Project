#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main()
{
    string EPS = "epsilon";
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);
    for (int i = 0; i < 8; i++)
        addr.sin_zero[i] = 0;

    if (bind(server, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "port 8080 is already in use, close the other program" << endl;
        return 1;
    }
    listen(server, 10);

    cout << "Server started. Open http://localhost:8080 in the browser" << endl;

    while (true)
    {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET)
        {
            continue;
        }
        DWORD wait = 1000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&wait, sizeof(wait));

        string data = "";
        char buf[4096];
        int headerEnd = -1;
        while (headerEnd == -1)
        {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0)
            {
                break;
            }
            data.append(buf, n);
            headerEnd = data.find("\r\n\r\n");
        }
        if (headerEnd == -1)
        {
            closesocket(client);
            continue;
        }

        string method, path;
        istringstream firstLine(data.substr(0, data.find("\r\n")));
        firstLine >> method >> path;

        string headers = data.substr(0, headerEnd);
        for (int i = 0; i < headers.size(); i++)
        {
            if (headers[i] >= 'A' && headers[i] <= 'Z')
            {
                headers[i] = headers[i] + 32;
            }
        }
        int contentLength = 0;
        int cl = headers.find("content-length:");
        if (cl != -1)
        {
            contentLength = atoi(headers.c_str() + cl + 15);
        }

        string body = data.substr(headerEnd + 4);
        while (body.size() < contentLength)
        {
            int n = recv(client, buf, sizeof(buf), 0);
            if (n <= 0)
            {
                break;
            }
            body.append(buf, n);
        }

        cout << method << " " << path << endl;

        int code = 404;
        string type = "text/plain";
        string content = "Not found";

        if (method == "POST" && path == "/process")
        {
            code = 200;
            type = "text/plain; charset=utf-8";

            string grammarText = body;
            string inputText = "";
            int mark = body.find("\n%%INPUT%%\n");
            if (mark != -1)
            {
                grammarText = body.substr(0, mark);
                inputText = body.substr(mark + 11);
            }

            map<string, vector<vector<string>>> g;
            vector<string> order;
            string err = "";

            vector<string> lhsList;
            vector<string> rhsList;
            vector<int> lineNumbers;

            istringstream in(grammarText);
            string line;
            int lineNo = 0;
            while (getline(in, line))
            {
                lineNo++;
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (line == "" || line[0] == '#')
                {
                    continue;
                }

                int arrow = line.find("->");
                if (arrow == -1)
                {
                    err = "Line " + to_string(lineNo) + ": missing '->'.";
                    break;
                }
                if (line.find("->", arrow + 2) != string::npos)
                {
                    err = "Line " + to_string(lineNo) + ": more than one '->' found.";
                    break;
                }

                string lhs = line.substr(0, arrow);
                string rhs = line.substr(arrow + 2);
                lhs.erase(0, lhs.find_first_not_of(" \t"));
                lhs.erase(lhs.find_last_not_of(" \t") + 1);
                rhs.erase(0, rhs.find_first_not_of(" \t"));
                rhs.erase(rhs.find_last_not_of(" \t") + 1);

                if (lhs == "" || lhs[0] < 'A' || lhs[0] > 'Z')
                {
                    err = "Line " + to_string(lineNo) + ": left side must be a non-terminal starting with a capital letter (like E).";
                    break;
                }
                if (lhs.find(' ') != string::npos || lhs.find('|') != string::npos)
                {
                    err = "Line " + to_string(lineNo) + ": left side must be a single non-terminal.";
                    break;
                }
                if (rhs == "")
                {
                    err = "Line " + to_string(lineNo) + ": right side is empty. Use epsilon for an empty production.";
                    break;
                }

                lhsList.push_back(lhs);
                rhsList.push_back(rhs);
                lineNumbers.push_back(lineNo);
            }

            if (err == "" && lhsList.size() == 0)
            {
                err = "Please enter at least one production.";
            }

            vector<vector<string>> altList;
            vector<string> nonTerminalNames = lhsList;
            sort(nonTerminalNames.begin(), nonTerminalNames.end(),
                 [](const string &a, const string &b)
                 {
                     return a.size() > b.size();
                 });

            for (int i = 0; i < rhsList.size() && err == ""; i++)
            {
                vector<string> alts;
                string rhs = rhsList[i] + "|";
                string alt = "";

                for (int j = 0; j < rhs.size(); j++)
                {
                    if (rhs[j] != '|')
                    {
                        alt = alt + rhs[j];
                        continue;
                    }

                    alt.erase(0, alt.find_first_not_of(" \t"));
                    if (alt.find_first_not_of(" \t") == string::npos)
                    {
                        err = "Line " + to_string(lineNumbers[i]) + ": empty alternative near '|'. Use epsilon for an empty production.";
                        break;
                    }
                    alt.erase(alt.find_last_not_of(" \t") + 1);
                    alts.push_back(alt);
                    alt = "";
                }
                altList.push_back(alts);
            }

            for (int i = 0; i < lhsList.size() && err == ""; i++)
            {
                string lhs = lhsList[i];
                if (g.count(lhs) == 0)
                {
                    order.push_back(lhs);
                    g[lhs];
                }

                for (int a = 0; a < altList[i].size() && err == ""; a++)
                {
                    string alt = altList[i][a];
                    vector<string> p;
                    int c = 0;

                    while (c < (int)alt.size())
                    {
                        if (alt[c] == ' ' || alt[c] == '\t' || alt[c] == '\r' || alt[c] == '\n')
                        {
                            c++;
                            continue;
                        }

                        bool matchedNonTerminal = false;
                        for (int n = 0; n < (int)nonTerminalNames.size(); n++)
                        {
                            const string &name = nonTerminalNames[n];
                            if (alt.compare(c, name.size(), name) == 0)
                            {
                                int afterName = c + (int)name.size();
                                bool endsAtSymbolBoundary = afterName == (int)alt.size() || !((alt[afterName] >= 'a' && alt[afterName] <= 'z') || (alt[afterName] >= 'A' && alt[afterName] <= 'Z') || (alt[afterName] >= '0' && alt[afterName] <= '9') || alt[afterName] == '_');

                                if (endsAtSymbolBoundary)
                                {
                                    p.push_back(name);
                                    c += (int)name.size();
                                    matchedNonTerminal = true;
                                    break;
                                }
                            }
                        }
                        if (matchedNonTerminal)
                        {
                            continue;
                        }

                        if (alt.compare(c, 7, "epsilon") == 0 && (c + 7 == (int)alt.size() || alt[c + 7] == ' ' || alt[c + 7] == '\t'))
                        {
                            p.push_back(EPS);
                            c += 7;
                            continue;
                        }
                        if (alt.compare(c, 3, "eps") == 0 && (c + 3 == (int)alt.size() || alt[c + 3] == ' ' || alt[c + 3] == '\t'))
                        {
                            p.push_back(EPS);
                            c += 3;
                            continue;
                        }

                        if ((alt[c] >= 'a' && alt[c] <= 'z') || (alt[c] >= 'A' && alt[c] <= 'Z') || (alt[c] >= '0' && alt[c] <= '9') || alt[c] == '_')
                        {
                            string token = "";
                            while (c < (int)alt.size() && ((alt[c] >= 'a' && alt[c] <= 'z') || (alt[c] >= 'A' && alt[c] <= 'Z') || (alt[c] >= '0' && alt[c] <= '9') || alt[c] == '_'))
                            {
                                token += alt[c];
                                c++;
                            }
                            p.push_back(token);
                        }
                        else
                        {
                            p.push_back(string(1, alt[c]));
                            c++;
                        }
                    }

                    for (int k = 0; k < p.size(); k++)
                    {
                        if (p[k] == "$")
                        {
                            err = "Line " + to_string(lineNumbers[i]) + ": '$' is the end marker, you cannot use it in the grammar.";
                        }
                        if (p[k] == EPS && p.size() > 1)
                        {
                            err = "Line " + to_string(lineNumbers[i]) + ": epsilon must be alone in its alternative.";
                        }
                    }

                    if (err == "" && find(g[lhs].begin(), g[lhs].end(), p) == g[lhs].end())
                    {
                        g[lhs].push_back(p);
                    }
                }
            }

            for (int i = 0; i < order.size() && err == ""; i++)
            {
                for (int j = 0; j < g[order[i]].size(); j++)
                {
                    for (int k = 0; k < g[order[i]][j].size(); k++)
                    {
                        string s = g[order[i]][j][k];
                        if (s[0] >= 'A' && s[0] <= 'Z' && g.count(s) == 0)
                        {
                            err = "Non-terminal '" + s + "' is used in a production but never defined.";
                        }
                    }
                }
            }

            vector<map<string, vector<vector<string>>>> savedG;
            vector<vector<string>> savedOrder;
            savedG.push_back(g);
            savedOrder.push_back(order);
            vector<string> names = order;
            for (int i = 0; i < names.size() && err == ""; i++)
            {
                string Ai = names[i];
                for (int j = 0; j < i; j++)
                {
                    string Aj = names[j];
                    vector<vector<string>> newProds;

                    for (int k = 0; k < g[Ai].size(); k++)
                    {
                        vector<string> p = g[Ai][k];

                        if (p[0] == Aj)
                        {
                            vector<string> rest(p.begin() + 1, p.end());
                            for (int m = 0; m < g[Aj].size(); m++)
                            {
                                vector<string> np;
                                if (g[Aj][m][0] != EPS)
                                {
                                    np = g[Aj][m];
                                }
                                np.insert(np.end(), rest.begin(), rest.end());
                                if (np.size() == 0)
                                {
                                    np.push_back(EPS);
                                }
                                if (find(newProds.begin(), newProds.end(), np) == newProds.end())
                                {
                                    newProds.push_back(np);
                                }
                            }
                        }
                        else
                        {
                            if (find(newProds.begin(), newProds.end(), p) == newProds.end())
                            {
                                newProds.push_back(p);
                            }
                        }
                    }
                    g[Ai] = newProds;
                }

                vector<vector<string>> alpha;
                vector<vector<string>> beta;
                bool leftRec = false;
                for (int k = 0; k < g[Ai].size(); k++)
                {
                    vector<string> p = g[Ai][k];
                    if (p[0] == Ai)
                    {
                        leftRec = true;
                        vector<string> rest(p.begin() + 1, p.end());
                        if (rest.size() > 0)
                        {
                            alpha.push_back(rest);
                        }
                    }
                    else
                    {
                        beta.push_back(p);
                    }
                }

                if (leftRec == false)
                {
                    continue;
                }

                if (beta.size() == 0)
                {
                    err = "Non-terminal '" + Ai + "' only has left-recursive productions, so it can never produce a string.";
                    break;
                }
                if (alpha.size() == 0)
                {
                    g[Ai] = beta;
                    continue;
                }
                string Anew = Ai + "'";
                while (g.count(Anew) > 0)
                    Anew = Anew + "'";
                order.insert(find(order.begin(), order.end(), Ai) + 1, Anew);
                vector<vector<string>> listA;
                vector<vector<string>> listAnew;
                for (int k = 0; k < beta.size(); k++)
                {
                    vector<string> p = beta[k];
                    if (p[0] == EPS)
                    {
                        p.clear();
                    }
                    p.push_back(Anew);
                    if (find(listA.begin(), listA.end(), p) == listA.end())
                    {
                        listA.push_back(p);
                    }
                }
                for (int k = 0; k < alpha.size(); k++)
                {
                    vector<string> p = alpha[k];
                    if (p[0] == EPS)
                    {
                        p.clear();
                    }
                    p.push_back(Anew);
                    if (find(listAnew.begin(), listAnew.end(), p) == listAnew.end())
                    {
                        listAnew.push_back(p);
                    }
                }
                listAnew.push_back(vector<string>(1, EPS));

                g[Ai] = listA;
                g[Anew] = listAnew;
            }

            string reply = "";

            if (err != "")
            {
                reply = "GRAMMAR ERROR\n" + err + "\n";
            }
            else
            {

                savedG.push_back(g);
                savedOrder.push_back(order);

                bool again = true;
                int count = 0;

                while (again && count < 200)
                {
                    again = false;
                    count++;
                    vector<string> all = order;

                    for (int n = 0; n < all.size() && !again; n++)
                    {
                        string A = all[n];
                        vector<vector<string>> prods = g[A];

                        map<string, vector<int>> groups;
                        for (int i = 0; i < prods.size(); i++)
                        {
                            if (prods[i][0] != EPS)
                            {
                                groups[prods[i][0]].push_back(i);
                            }
                        }

                        for (pair<string, vector<int>> grp : groups)
                        {
                            vector<int> mem = grp.second;
                            if (mem.size() < 2)
                            {
                                continue;
                            }

                            int len = prods[mem[0]].size();
                            for (int m = 1; m < mem.size(); m++)
                            {
                                int k = 0;
                                while (k < len && k < prods[mem[m]].size() && prods[mem[m]][k] == prods[mem[0]][k])
                                    k++;
                                len = k;
                            }
                            vector<string> prefix(prods[mem[0]].begin(), prods[mem[0]].begin() + len);

                            string Anew = A + "'";
                            while (g.count(Anew) > 0)
                                Anew = Anew + "'";
                            order.insert(find(order.begin(), order.end(), A) + 1, Anew);

                            vector<vector<string>> listA;
                            vector<vector<string>> listAnew;

                            for (int i = 0; i < prods.size(); i++)
                            {
                                bool inGroup = find(mem.begin(), mem.end(), i) != mem.end();

                                if (inGroup == false)
                                {
                                    if (find(listA.begin(), listA.end(), prods[i]) == listA.end())
                                    {
                                        listA.push_back(prods[i]);
                                    }
                                }
                                else
                                {
                                    if (i == mem[0])
                                    {
                                        vector<string> np = prefix;
                                        np.push_back(Anew);
                                        if (find(listA.begin(), listA.end(), np) == listA.end())
                                        {
                                            listA.push_back(np);
                                        }
                                    }
                                    vector<string> suffix(prods[i].begin() + len, prods[i].end());
                                    if (suffix.size() == 0)
                                    {
                                        suffix.push_back(EPS);
                                    }
                                    if (find(listAnew.begin(), listAnew.end(), suffix) == listAnew.end())
                                    {
                                        listAnew.push_back(suffix);
                                    }
                                }
                            }

                            g[A] = listA;
                            g[Anew] = listAnew;
                            again = true;
                            break;
                        }
                    }
                }

                savedG.push_back(g);
                savedOrder.push_back(order);

                map<string, vector<string>> prodText;
                for (int i = 0; i < order.size(); i++)
                {
                    for (int j = 0; j < g[order[i]].size(); j++)
                    {
                        string text = "";
                        for (int k = 0; k < g[order[i]][j].size(); k++)
                        {
                            if (k > 0)
                            {
                                text = text + " ";
                            }
                            text = text + g[order[i]][j][k];
                        }
                        prodText[order[i]].push_back(text);
                    }
                }

                map<string, set<string>> first;
                map<string, set<string>> follow;

                bool changed = true;
                while (changed)
                {
                    changed = false;

                    for (int i = 0; i < order.size(); i++)
                    {
                        string A = order[i];

                        for (int j = 0; j < g[A].size(); j++)
                        {
                            vector<string> p = g[A][j];
                            bool allEmpty = true;

                            for (int k = 0; k < p.size(); k++)
                            {
                                string s = p[k];
                                if (s == EPS)
                                {
                                    continue;
                                }

                                if (g.count(s) == 0)
                                {
                                    if (first[A].insert(s).second)
                                    {
                                        changed = true;
                                    }
                                    allEmpty = false;
                                    break;
                                }

                                bool hasEps = false;
                                for (string x : first[s])
                                {
                                    if (x == EPS)
                                    {
                                        hasEps = true;
                                    }
                                    else if (first[A].insert(x).second)
                                    {
                                        changed = true;
                                    }
                                }
                                if (hasEps == false)
                                {
                                    allEmpty = false;
                                    break;
                                }
                            }

                            if (allEmpty)
                            {
                                if (first[A].insert(EPS).second)
                                {
                                    changed = true;
                                }
                            }
                        }
                    }
                }

                follow[order[0]].insert("$");

                changed = true;
                while (changed)
                {
                    changed = false;

                    for (int i = 0; i < order.size(); i++)
                    {
                        string A = order[i];

                        for (int j = 0; j < g[A].size(); j++)
                        {
                            vector<string> p = g[A][j];

                            for (int k = 0; k < p.size(); k++)
                            {
                                string B = p[k];
                                if (g.count(B) == 0)
                                {
                                    continue;
                                }

                                bool betaEmpty = true;

                                for (int m = k + 1; m < p.size(); m++)
                                {
                                    string s = p[m];
                                    if (s == EPS)
                                    {
                                        continue;
                                    }

                                    if (g.count(s) == 0)
                                    {
                                        if (follow[B].insert(s).second)
                                        {
                                            changed = true;
                                        }
                                        betaEmpty = false;
                                        break;
                                    }

                                    bool hasEps = false;
                                    for (string x : first[s])
                                    {
                                        if (x == EPS)
                                        {
                                            hasEps = true;
                                        }
                                        else if (follow[B].insert(x).second)
                                        {
                                            changed = true;
                                        }
                                    }
                                    if (hasEps == false)
                                    {
                                        betaEmpty = false;
                                        break;
                                    }
                                }

                                if (betaEmpty)
                                {
                                    set<string> fa = follow[A];
                                    for (string x : fa)
                                    {
                                        if (follow[B].insert(x).second)
                                        {
                                            changed = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                map<string, map<string, vector<int>>> table;

                set<string> termSet;
                for (int i = 0; i < order.size(); i++)
                    for (int j = 0; j < g[order[i]].size(); j++)
                        for (int k = 0; k < g[order[i]][j].size(); k++)
                        {
                            string s = g[order[i]][j][k];
                            if (s != EPS && g.count(s) == 0)
                            {
                                termSet.insert(s);
                            }
                        }
                vector<string> terminals(termSet.begin(), termSet.end());
                terminals.push_back("$");

                for (int i = 0; i < order.size(); i++)
                {
                    string A = order[i];

                    for (int j = 0; j < g[A].size(); j++)
                    {
                        vector<string> p = g[A][j];

                        set<string> f;
                        bool allEmpty = true;
                        for (int k = 0; k < p.size(); k++)
                        {
                            string s = p[k];
                            if (s == EPS)
                            {
                                continue;
                            }

                            if (g.count(s) == 0)
                            {
                                f.insert(s);
                                allEmpty = false;
                                break;
                            }

                            bool hasEps = false;
                            for (string x : first[s])
                            {
                                if (x == EPS)
                                {
                                    hasEps = true;
                                }
                                else
                                {
                                    f.insert(x);
                                }
                            }
                            if (hasEps == false)
                            {
                                allEmpty = false;
                                break;
                            }
                        }

                        if (allEmpty)
                        {
                            for (string x : follow[A])
                                f.insert(x);
                        }

                        for (string x : f)
                            table[A][x].push_back(j);
                    }
                }

                vector<string> conflicts;
                for (int i = 0; i < order.size(); i++)
                {
                    for (int c = 0; c < terminals.size(); c++)
                    {
                        string A = order[i];
                        string a = terminals[c];
                        if (table[A].count(a) > 0 && table[A][a].size() > 1)
                        {
                            string msg = "M[" + A + ", " + a + "] has " + to_string(table[A][a].size()) + " productions: ";
                            for (int k = 0; k < table[A][a].size(); k++)
                            {
                                if (k > 0)
                                {
                                    msg = msg + "  AND  ";
                                }
                                msg = msg + A + " -> " + prodText[A][table[A][a][k]];
                            }
                            conflicts.push_back(msg);
                        }
                    }
                }

                vector<vector<string>> trace;
                string parseMessage = "";
                string resultText = "";
                string treeText = "";
                string note = "";

                if (conflicts.size() > 0)
                {
                    parseMessage = "Parsing skipped: the grammar is not LL(1) (see the conflicts above).\n";
                }
                else
                {

                    vector<string> tokens;
                    istringstream inStream(inputText);
                    string chunk;
                    while (inStream >> chunk)
                    {
                        int i = 0;
                        while (i < chunk.size())
                        {
                            int bestLen = 0;
                            string best = "";
                            for (int t = 0; t < terminals.size(); t++)
                            {
                                if (terminals[t] != "$" && terminals[t].size() > bestLen && chunk.compare(i, terminals[t].size(), terminals[t]) == 0)
                                {
                                    bestLen = terminals[t].size();
                                    best = terminals[t];
                                }
                            }
                            if (bestLen == 0)
                            {
                                tokens.push_back(string(1, chunk[i]));
                                i++;
                            }
                            else
                            {
                                tokens.push_back(best);
                                i = i + bestLen;
                            }
                        }
                    }
                    tokens.push_back("$");
                    if (tokens.size() == 1)
                    {
                        note = "(input is empty, so the empty string is being tested)\n";
                    }

                    vector<string> treeName;
                    vector<vector<int>> treeKids;
                    treeName.push_back(order[0]);
                    treeKids.push_back(vector<int>());

                    vector<string> stackSym;
                    vector<int> stackNode;
                    stackSym.push_back("$");
                    stackNode.push_back(-1);
                    stackSym.push_back(order[0]);
                    stackNode.push_back(0);

                    vector<string> header;
                    header.push_back("Step");
                    header.push_back("Stack (top on left)");
                    header.push_back("Remaining input");
                    header.push_back("Action");
                    trace.push_back(header);

                    int pos = 0;
                    int step = 0;
                    bool accepted = false;
                    string reason = "";

                    while (step < 5000)
                    {
                        step++;
                        string top = stackSym.back();
                        string look = tokens[pos];

                        string stackText = "";
                        for (int i = stackSym.size() - 1; i >= 0; i--)
                        {
                            if (i < stackSym.size() - 1)
                            {
                                stackText = stackText + " ";
                            }
                            stackText = stackText + stackSym[i];
                        }
                        string restText = "";
                        for (int i = pos; i < tokens.size(); i++)
                        {
                            if (i > pos)
                            {
                                restText = restText + " ";
                            }
                            restText = restText + tokens[i];
                        }

                        vector<string> row;
                        row.push_back(to_string(step));
                        row.push_back(stackText);
                        row.push_back(restText);

                        if (top == "$" && look == "$")
                        {
                            row.push_back("ACCEPT");
                            trace.push_back(row);
                            accepted = true;
                            break;
                        }

                        if (g.count(top) == 0)
                        {
                            if (top == look)
                            {
                                row.push_back("match " + look);
                                trace.push_back(row);
                                stackSym.pop_back();
                                stackNode.pop_back();
                                pos++;
                                continue;
                            }
                            row.push_back("ERROR");
                            trace.push_back(row);
                            reason = "expected '" + top + "' but found '" + look + "'.";
                            break;
                        }

                        if (table[top].count(look) == 0)
                        {
                            row.push_back("ERROR");
                            trace.push_back(row);
                            reason = "table entry M[" + top + ", " + look + "] is empty.";
                            break;
                        }

                        int which = table[top][look][0];
                        vector<string> p = g[top][which];
                        row.push_back(top + " -> " + prodText[top][which]);
                        trace.push_back(row);

                        int parent = stackNode.back();
                        stackSym.pop_back();
                        stackNode.pop_back();

                        if (p[0] == EPS)
                        {
                            treeName.push_back(EPS);
                            treeKids.push_back(vector<int>());
                            treeKids[parent].push_back(treeName.size() - 1);
                        }
                        else
                        {
                            vector<int> kids;
                            for (int i = 0; i < p.size(); i++)
                            {
                                treeName.push_back(p[i]);
                                treeKids.push_back(vector<int>());
                                kids.push_back(treeName.size() - 1);
                                treeKids[parent].push_back(treeName.size() - 1);
                            }

                            for (int i = p.size() - 1; i >= 0; i--)
                            {
                                stackSym.push_back(p[i]);
                                stackNode.push_back(kids[i]);
                            }
                        }
                    }

                    if (accepted == false && reason == "")
                    {
                        reason = "too many steps (maybe hidden left recursion).";
                    }

                    if (accepted)
                    {
                        resultText = "\nResult: ACCEPTED - the string belongs to the language.\n";

                        vector<int> todo;
                        vector<int> depths;
                        todo.push_back(0);
                        depths.push_back(0);

                        while (todo.size() > 0)
                        {
                            int node = todo.back();
                            int depth = depths.back();
                            todo.pop_back();
                            depths.pop_back();

                            if (depth == 0)
                            {
                                treeText = treeText + treeName[node] + "\n";
                            }
                            else
                            {
                                treeText = treeText + string((depth - 1) * 4, ' ') + "|-- " + treeName[node] + "\n";
                            }

                            for (int c = treeKids[node].size() - 1; c >= 0; c--)
                            {
                                todo.push_back(treeKids[node][c]);
                                depths.push_back(depth + 1);
                            }
                        }
                    }
                    else
                    {
                        resultText = "\nResult: REJECTED - " + reason + "\n";
                    }
                }

                vector<vector<string>> tableRows;
                vector<string> tableHeader;
                tableHeader.push_back("");
                for (int c = 0; c < terminals.size(); c++)
                    tableHeader.push_back(terminals[c]);
                tableRows.push_back(tableHeader);

                for (int i = 0; i < order.size(); i++)
                {
                    string A = order[i];
                    vector<string> row;
                    row.push_back(A);
                    for (int c = 0; c < terminals.size(); c++)
                    {
                        string cell = "";
                        string a = terminals[c];
                        if (table[A].count(a) > 0)
                        {
                            for (int k = 0; k < table[A][a].size(); k++)
                            {
                                if (k > 0)
                                {
                                    cell = cell + " ; ";
                                }
                                cell = cell + A + "->" + prodText[A][table[A][a][k]];
                            }
                        }
                        row.push_back(cell);
                    }
                    tableRows.push_back(row);
                }
                string tableOut = "";
                string traceOut = "";
                for (int w = 0; w < 2; w++)
                {
                    vector<vector<string>> rows = tableRows;
                    if (w == 1)
                    {
                        rows = trace;
                    }
                    if (rows.size() == 0)
                    {
                        continue;
                    }

                    vector<int> width(rows[0].size(), 0);
                    for (int r = 0; r < rows.size(); r++)
                        for (int c = 0; c < rows[r].size(); c++)
                            if (rows[r][c].size() > width[c])
                            {
                                width[c] = rows[r][c].size();
                            }

                    string text = "";
                    for (int r = 0; r < rows.size(); r++)
                    {
                        for (int c = 0; c < rows[r].size(); c++)
                        {
                            text = text + rows[r][c] + string(width[c] - rows[r][c].size(), ' ');
                            if (c < rows[r].size() - 1)
                            {
                                text = text + " | ";
                            }
                        }
                        text = text + "\n";

                        if (r == 0)
                        {
                            int total = 0;
                            for (int c = 0; c < width.size(); c++)
                                total = total + width[c] + 3;
                            if (w == 1)
                            {
                                total = total - 3;
                            }
                            text = text + string(total, '-') + "\n";
                        }
                    }

                    if (w == 0)
                    {
                        tableOut = text;
                    }
                    else
                    {
                        traceOut = text;
                    }
                }

                string out = "PREDICTIVE PARSER GENERATOR\n" + string(70, '=') + "\n\n";

                string stageName[3] = {"ORIGINAL GRAMMAR", "AFTER LEFT RECURSION ELIMINATION", "AFTER LEFT FACTORING (final grammar)"};
                for (int s = 0; s < 3; s++)
                {
                    out = out + stageName[s] + "\n";
                    for (int i = 0; i < savedOrder[s].size(); i++)
                    {
                        string name = savedOrder[s][i];
                        out = out + name + " -> ";
                        for (int j = 0; j < savedG[s][name].size(); j++)
                        {
                            if (j > 0)
                            {
                                out = out + " | ";
                            }
                            for (int k = 0; k < savedG[s][name][j].size(); k++)
                            {
                                if (k > 0)
                                {
                                    out = out + " ";
                                }
                                out = out + savedG[s][name][j][k];
                            }
                        }
                        out = out + "\n";
                    }
                    out = out + "\n";
                }

                out = out + "NON-TERMINALS\n";
                for (int i = 0; i < order.size(); i++)
                {
                    if (i > 0)
                    {
                        out = out + ", ";
                    }
                    out = out + order[i];
                }
                out = out + "\n\nTERMINALS\n";
                if (terminals.size() == 1)
                {
                    out = out + "(none)";
                }
                for (int i = 0; i < terminals.size() - 1; i++)
                {
                    if (i > 0)
                    {
                        out = out + ", ";
                    }
                    out = out + terminals[i];
                }
                out = out + "\n\n";

                out = out + "FIRST SETS\n";
                for (int i = 0; i < order.size(); i++)
                {
                    out = out + "FIRST(" + order[i] + ") = { ";
                    bool firstOne = true;
                    for (string x : first[order[i]])
                    {
                        if (!firstOne)
                        {
                            out = out + ", ";
                        }
                        out = out + x;
                        firstOne = false;
                    }
                    out = out + " }\n";
                }
                out = out + "\nFOLLOW SETS\n";
                for (int i = 0; i < order.size(); i++)
                {
                    out = out + "FOLLOW(" + order[i] + ") = { ";
                    bool firstOne = true;
                    for (string x : follow[order[i]])
                    {
                        if (!firstOne)
                        {
                            out = out + ", ";
                        }
                        out = out + x;
                        firstOne = false;
                    }
                    out = out + " }\n";
                }

                out = out + "\nLL(1) PARSING TABLE\n" + tableOut + "\n";

                out = out + "LL(1) STATUS: ";
                if (conflicts.size() == 0)
                {
                    out = out + "YES - no conflicts, the grammar is LL(1).\n";
                }
                else
                {
                    out = out + "NO - conflicts found, the grammar is not LL(1).\n";
                    for (int i = 0; i < conflicts.size(); i++)
                        out = out + "  - " + conflicts[i] + "\n";
                }

                out = out + "\nINPUT PARSING TRACE\n" + string(70, '-') + "\n";
                if (parseMessage != "")
                {
                    out = out + parseMessage;
                }
                else
                {
                    out = out + note + traceOut + resultText;
                    if (treeText != "")
                    {
                        out = out + "\nPARSE TREE\n" + string(70, '-') + "\n" + treeText;
                    }
                }

                reply = out;
            }

            content = reply;
        }
        else if (method == "GET")
        {

            string file = "";
            if (path == "/" || path == "/index.html")
            {
                file = "index.html";
                type = "text/html; charset=utf-8";
            }
            else if (path == "/style.css")
            {
                file = "style.css";
                type = "text/css; charset=utf-8";
            }
            else if (path == "/script.js")
            {
                file = "script.js";
                type = "application/javascript; charset=utf-8";
            }

            if (file != "")
            {
                ifstream f(file.c_str(), ios::binary);
                if (f)
                {
                    stringstream ss;
                    ss << f.rdbuf();
                    content = ss.str();
                    code = 200;
                }
                else
                {
                    content = "Could not find " + file + ". Run the program from the project folder.";
                }
            }
        }
        else
        {
            code = 400;
            content = "Bad request";
        }

        string status = "404 Not Found";
        if (code == 200)
        {
            status = "200 OK";
        }
        if (code == 400)
        {
            status = "400 Bad Request";
        }

        string answer = "HTTP/1.1 " + status + "\r\n";
        answer = answer + "Content-Type: " + type + "\r\n";
        answer = answer + "Content-Length: " + to_string(content.size()) + "\r\n";
        answer = answer + "Connection: close\r\n\r\n";
        answer = answer + content;

        int sent = 0;
        while (sent < answer.size())
        {
            int n = send(client, answer.c_str() + sent, answer.size() - sent, 0);
            if (n <= 0)
            {
                break;
            }
            sent = sent + n;
        }

        closesocket(client);
    }

    return 0;
}
