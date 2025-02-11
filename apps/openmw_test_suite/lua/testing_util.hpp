#ifndef LUA_TESTING_UTIL_H
#define LUA_TESTING_UTIL_H

#include <sstream>
#include <sol/sol.hpp>

#include <components/vfs/archive.hpp>
#include <components/vfs/manager.hpp>

namespace
{

    template <typename T>
    T get(sol::state& lua, const std::string& luaCode)
    {
        return lua.safe_script("return " + luaCode).get<T>();
    }

    class TestFile : public VFS::File
    {
    public:
        explicit TestFile(std::string content) : mContent(std::move(content)) {}

        Files::IStreamPtr open() override
        {
            return std::make_unique<std::stringstream>(mContent, std::ios_base::in);
        }

        std::string getPath() override
        {
            return "TestFile";
        }

    private:
        const std::string mContent;
    };

    struct TestData : public VFS::Archive
    {
        std::map<std::string, VFS::File*> mFiles;

        TestData(std::map<std::string, VFS::File*> files) : mFiles(std::move(files)) {}

        void listResources(std::map<std::string, VFS::File*>& out, char (*normalize_function) (char)) override
        {
            out = mFiles;
        }

        bool contains(const std::string& file, char (*normalize_function) (char)) const override
        {
            return mFiles.count(file) != 0;
        }

        std::string getDescription() const override { return "TestData"; }

    };

    inline std::unique_ptr<VFS::Manager> createTestVFS(std::map<std::string, VFS::File*> files)
    {
        auto vfs = std::make_unique<VFS::Manager>(true);
        vfs->addArchive(new TestData(std::move(files)));
        vfs->buildIndex();
        return vfs;
    }

    #define EXPECT_ERROR(X, ERR_SUBSTR) try { X; FAIL() << "Expected error"; } \
        catch (std::exception& e) { EXPECT_THAT(e.what(), ::testing::HasSubstr(ERR_SUBSTR)); }

}

#endif // LUA_TESTING_UTIL_H
