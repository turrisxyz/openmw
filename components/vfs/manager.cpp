#include "manager.hpp"

#include <stdexcept>

#include <components/misc/stringops.hpp>

#include "archive.hpp"

namespace
{

    char strict_normalize_char(char ch)
    {
        return ch == '\\' ? '/' : ch;
    }

    char nonstrict_normalize_char(char ch)
    {
        return ch == '\\' ? '/' : Misc::StringUtils::toLower(ch);
    }

    void normalize_path(std::string& path, bool strict)
    {
        char (*normalize_char)(char) = strict ? &strict_normalize_char : &nonstrict_normalize_char;
        std::transform(path.begin(), path.end(), path.begin(), normalize_char);
    }

}

namespace VFS
{

    Manager::Manager(bool strict)
        : mStrict(strict)
    {

    }

    Manager::~Manager()
    {
        reset();
    }

    void Manager::reset()
    {
        mIndex.clear();
        for (std::vector<Archive*>::iterator it = mArchives.begin(); it != mArchives.end(); ++it)
            delete *it;
        mArchives.clear();
    }

    void Manager::addArchive(Archive *archive)
    {
        mArchives.push_back(archive);
    }

    void Manager::buildIndex()
    {
        mIndex.clear();

        for (std::vector<Archive*>::const_iterator it = mArchives.begin(); it != mArchives.end(); ++it)
            (*it)->listResources(mIndex, mStrict ? &strict_normalize_char : &nonstrict_normalize_char);
    }

    Files::IStreamPtr Manager::get(const std::string &name) const
    {
        std::string normalized = name;
        normalize_path(normalized, mStrict);

        return getNormalized(normalized);
    }

    Files::IStreamPtr Manager::getNormalized(const std::string &normalizedName) const
    {
        std::map<std::string, File*>::const_iterator found = mIndex.find(normalizedName);
        if (found == mIndex.end())
            throw std::runtime_error("Resource '" + normalizedName + "' not found");
        return found->second->open();
    }

    bool Manager::exists(const std::string &name) const
    {
        std::string normalized = name;
        normalize_path(normalized, mStrict);

        return mIndex.find(normalized) != mIndex.end();
    }

    std::string Manager::normalizeFilename(const std::string& name) const
    {
        std::string result = name;
        normalize_path(result, mStrict);
        return result;
    }

    std::string Manager::getArchive(const std::string& name) const
    {
        std::string normalized = name;
        normalize_path(normalized, mStrict);
        for(auto it = mArchives.rbegin(); it != mArchives.rend(); ++it)
        {
            if((*it)->contains(normalized, mStrict ? &strict_normalize_char : &nonstrict_normalize_char))
                return (*it)->getDescription();
        }
        return {};
    }

    std::string Manager::getAbsoluteFileName(const std::string& name) const
    {
        std::string normalized = name;
        normalize_path(normalized, mStrict);

        std::map<std::string, File*>::const_iterator found = mIndex.find(normalized);
        if (found == mIndex.end())
            throw std::runtime_error("Resource '" + normalized + "' not found");
        return found->second->getPath();
    }

    namespace
    {
        bool startsWith(std::string_view text, std::string_view start)
        {
            return text.rfind(start, 0) == 0;
        }
    }

    Manager::RecursiveDirectoryRange Manager::getRecursiveDirectoryIterator(const std::string& path) const
    {
        if (path.empty())
            return { mIndex.begin(), mIndex.end() };
        auto normalized = normalizeFilename(path);
        const auto it = mIndex.lower_bound(normalized);
        if (it == mIndex.end() || !startsWith(it->first, normalized))
            return { it, it };
        ++normalized.back();
        return { it, mIndex.lower_bound(normalized) };
    }
}
