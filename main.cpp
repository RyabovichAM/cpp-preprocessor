#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool PreprocessInternal(const path&, ofstream&, const vector<path>&);

bool CheckDirectories(ifstream& in_file, ofstream& out_file, const string& smatch_path, const vector<path>& include_directories) {
    for (const auto& incl_dir : include_directories) {
        if(filesystem::exists(path( incl_dir / smatch_path))) {
            path found_path = incl_dir / smatch_path;
            in_file.open(found_path);
            if(!PreprocessInternal(found_path, out_file, include_directories)) {
                return false;
            }
            break;
        }
    }
    return true;
}
bool FileOpenInfo(ifstream& file_to_check, const string& filename_where,const smatch& sm, size_t line_counter) {
    if(!file_to_check.is_open()) {
        cout << "unknown include file "s << sm[1] << " at file "s << filename_where << " at line " << line_counter << endl;
        return false;
    }
    return true;
}

bool PreprocessInternal(const path& in_file, ofstream& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);

    static const regex local_includes_reg(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static const regex global_includes_reg(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch sm;

    string input_line;
    size_t line_counter = 0;
    while(getline(in, input_line)) {
        ++line_counter;

        if(regex_match(input_line,sm,local_includes_reg)) {
            ifstream local;
            path found_path = in_file.parent_path() / sm[1].str();
            if(filesystem::exists(found_path)) {
                local.open(found_path);
                if(!PreprocessInternal(found_path, out_file, include_directories)) {
                    return false;
                }
            } else {
                if(!CheckDirectories(local, out_file, sm[1].str(), include_directories)) {
                    return false;
                }
            }

            if(!FileOpenInfo(local, in_file.string(), sm, line_counter)) {
                return false;
            }

            local.close();
            continue;
        }

        if(regex_match(input_line,sm,global_includes_reg)) {
            ifstream local;
            path found_path;

            if(!CheckDirectories(local, out_file,sm[1].str(), include_directories)) {
                return false;
            }

            if(!FileOpenInfo(local, in_file.string(), sm, line_counter)) {
                return false;
            }

            local.close();
            continue;
        }

        out_file  << input_line << '\n';
    };
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if(!in) {
        return false;
    }

    ofstream out(out_file,std::ios::app);
    if(!out) {
        return false;
    }

    if(!PreprocessInternal(in_file,out,include_directories)) {
        return false;
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
