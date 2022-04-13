#include "postprocessorhud.hpp"

#include <MyGUI_Window.h>
#include <MyGUI_Button.h>
#include <MyGUI_TabItem.h>
#include <MyGUI_TabControl.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_FactoryManager.h>

#include <components/fx/widgets.hpp>
#include <components/fx/technique.hpp>

#include <components/widgets/box.hpp>

#include "../mwrender/postprocessor.hpp"

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace
{
    void saveChain()
    {
        auto* processor = MWBase::Environment::get().getWorld()->getPostProcessor();

        std::ostringstream chain;

        for (size_t i = 1; i < processor->getTechniques().size(); ++i)
        {
            auto technique = processor->getTechniques()[i];

            if (!technique)
                continue;

            chain << technique->getName();

            if (i < processor-> getTechniques().size() - 1)
                chain << ",";
        }

        Settings::Manager::setString("chain", "Post Processing", chain.str());
    }
}

namespace MWGui
{
    void PostProcessorHud::ListWrapper::onKeyButtonPressed(MyGUI::KeyCode key, MyGUI::Char ch)
    {
        if (MyGUI::InputManager::getInstance().isShiftPressed() && (key == MyGUI::KeyCode::ArrowUp || key == MyGUI::KeyCode::ArrowDown))
            return;

        MyGUI::ListBox::onKeyButtonPressed(key, ch);
    }

    PostProcessorHud::PostProcessorHud()
        : WindowBase("openmw_postprocessor_hud.layout")
    {
        MyGUI::IntSize viewSize = MyGUI::RenderManager::getInstance().getViewSize();
        mMainWidget->setSize({std::min(600, viewSize.width), viewSize.height - 4});

        getWidget(mTabConfiguration, "TabConfiguration");
        getWidget(mActiveList, "ActiveList");
        getWidget(mInactiveList, "InactiveList");
        getWidget(mModeToggle, "ModeToggle");
        getWidget(mConfigLayout, "ConfigLayout");
        getWidget(mFilter, "Filter");

        mActiveList->eventKeyButtonPressed += MyGUI::newDelegate(this, &PostProcessorHud::notifyKeyButtonPressed);
        mInactiveList->eventKeyButtonPressed += MyGUI::newDelegate(this, &PostProcessorHud::notifyKeyButtonPressed);

        mActiveList->eventListChangePosition += MyGUI::newDelegate(this, &PostProcessorHud::notifyListChangePosition);
        mInactiveList->eventListChangePosition += MyGUI::newDelegate(this, &PostProcessorHud::notifyListChangePosition);

        mModeToggle->eventMouseButtonClick += MyGUI::newDelegate(this, &PostProcessorHud::notifyModeToggle);

        mFilter->eventEditTextChange += MyGUI::newDelegate(this, &PostProcessorHud::notifyFilterChanged);

        mMainWidget->castType<MyGUI::Window>()->eventWindowChangeCoord += MyGUI::newDelegate(this, &PostProcessorHud::notifyWindowResize);

        mShaderInfo = mConfigLayout->createWidget<Gui::AutoSizedEditBox>("HeaderText", {}, MyGUI::Align::Default);
        mShaderInfo->setUserString("VStretch", "true");
        mShaderInfo->setUserString("HStretch", "true");
        mShaderInfo->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
        mShaderInfo->setEditReadOnly(true);
        mShaderInfo->setEditWordWrap(true);
        mShaderInfo->setEditMultiLine(true);

        mConfigLayout->setVisibleVScroll(true);

        mConfigArea = mConfigLayout->createWidget<MyGUI::Widget>("", {}, MyGUI::Align::Default);
    }

    void PostProcessorHud::notifyFilterChanged(MyGUI::EditBox* sender)
    {
        updateTechniques();
    }

    void PostProcessorHud::notifyWindowResize(MyGUI::Window* sender)
    {
        layout();
    }

    void PostProcessorHud::notifyResetButtonClicked(MyGUI::Widget* sender)
    {
        for (size_t i = 1; i < mConfigArea->getChildCount(); ++i)
        {
            if (auto* child = dynamic_cast<fx::Widgets::UniformBase*>(mConfigArea->getChildAt(i)))
                child->toDefault();
        }
    }

    void PostProcessorHud::notifyListChangePosition(MyGUI::ListBox* sender, size_t index)
    {
        if (sender == mActiveList)
            mInactiveList->clearIndexSelected();
        else if (sender == mInactiveList)
            mActiveList->clearIndexSelected();

        if (index >= sender->getItemCount())
            return;

        updateConfigView(sender->getItemNameAt(index));
    }

    void PostProcessorHud::notifyKeyButtonPressed(MyGUI::Widget* sender, MyGUI::KeyCode key, MyGUI::Char ch)
    {
        auto* processor = MWBase::Environment::get().getWorld()->getPostProcessor();

        MyGUI::ListBox* list = static_cast<MyGUI::ListBox*>(sender);

        size_t selected = list->getIndexSelected();

        if (selected == MyGUI::ITEM_NONE)
            return;

        if (key == MyGUI::KeyCode::ArrowLeft && list == mActiveList)
        {
            if (MyGUI::InputManager::getInstance().isShiftPressed())
            {
                MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mInactiveList);
                mActiveList->clearIndexSelected();
                select(mInactiveList, 0);
            }
            else
            {
                mOverrideHint = mActiveList->getItemNameAt(selected);
                processor->disableTechnique(*mActiveList->getItemDataAt<std::shared_ptr<fx::Technique>>(selected));
                saveChain();
            }
        }
        else if (key == MyGUI::KeyCode::ArrowRight && list == mInactiveList)
        {
            if (MyGUI::InputManager::getInstance().isShiftPressed())
            {
                MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mActiveList);
                mInactiveList->clearIndexSelected();
                select(mActiveList, 0);
            }
            else
            {
                mOverrideHint = mInactiveList->getItemNameAt(selected);
                processor->enableTechnique(*mInactiveList->getItemDataAt<std::shared_ptr<fx::Technique>>(selected));
                saveChain();
            }
        }
        else if (list == mActiveList && MyGUI::InputManager::getInstance().isShiftPressed() && (key == MyGUI::KeyCode::ArrowUp || key == MyGUI::KeyCode::ArrowDown))
        {
            int index = key == MyGUI::KeyCode::ArrowUp ? static_cast<int>(selected) - 1 : selected + 1;
            index = std::clamp<int>(index, 0, mActiveList->getItemCount() - 1);

            if ((size_t)index != selected)
            {
                if (processor->enableTechnique(*mActiveList->getItemDataAt<std::shared_ptr<fx::Technique>>(selected), index))
                    saveChain();
            }
        }
    }

    void PostProcessorHud::notifyModeToggle(MyGUI::Widget* sender)
    {
        Settings::ShaderManager::Mode prev = Settings::ShaderManager::getMode();
        toggleMode(prev == Settings::ShaderManager::Mode::Debug ? Settings::ShaderManager::Mode::Normal : Settings::ShaderManager::Mode::Debug);
    }

    void PostProcessorHud::onOpen()
    {
        updateTechniques();
    }

    void PostProcessorHud::onClose()
    {
        toggleMode(Settings::ShaderManager::Mode::Normal);
    }

    void PostProcessorHud::layout()
    {
        constexpr int padding = 12;
        constexpr int padding2 = padding * 2;
        mShaderInfo->setCoord(padding, padding, mConfigLayout->getSize().width - padding2 - padding, mShaderInfo->getTextSize().height);

        int totalHeight = mShaderInfo->getTop() + mShaderInfo->getTextSize().height + padding;

        mConfigArea->setCoord({padding, totalHeight, mShaderInfo->getSize().width, mConfigLayout->getHeight()});

        int childHeights = 0;
		MyGUI::EnumeratorWidgetPtr enumerator = mConfigArea->getEnumerator();
		while (enumerator.next())
		{
			enumerator.current()->setCoord(padding, childHeights + padding, mShaderInfo->getSize().width - padding2, enumerator.current()->getHeight());
            childHeights += enumerator.current()->getHeight() + padding;
		}
        totalHeight += childHeights;

        mConfigArea->setSize(mConfigArea->getWidth(), childHeights);

        mConfigLayout->setCanvasSize(mConfigLayout->getWidth() - padding2, totalHeight);
        mConfigLayout->setSize(mConfigLayout->getWidth(), mConfigLayout->getParentSize().height - padding2);
    }

    void PostProcessorHud::select(ListWrapper* list, size_t index)
    {
        list->setIndexSelected(index);
        notifyListChangePosition(list, index);
    }

    void PostProcessorHud::toggleMode(Settings::ShaderManager::Mode mode)
    {
        Settings::ShaderManager::setMode(mode);

        mModeToggle->setCaptionWithReplacing(mode == Settings::ShaderManager::Mode::Debug ? "#{sOn}" :"#{sOff}");

        MWBase::Environment::get().getWorld()->getPostProcessor()->toggleMode();

        if (!isVisible())
            return;

        if (mInactiveList->getIndexSelected() != MyGUI::ITEM_NONE)
            updateConfigView(mInactiveList->getItemNameAt(mInactiveList->getIndexSelected()));
        else if (mActiveList->getIndexSelected() != MyGUI::ITEM_NONE)
            updateConfigView(mActiveList->getItemNameAt(mActiveList->getIndexSelected()));
    }

    void PostProcessorHud::updateConfigView(const std::string& name)
    {
        auto* processor = MWBase::Environment::get().getWorld()->getPostProcessor();

        auto technique = processor->loadTechnique(name);

        if (!technique)
            return;

        while (mConfigArea->getChildCount() > 0)
            MyGUI::Gui::getInstance().destroyWidget(mConfigArea->getChildAt(0));

        mShaderInfo->setCaption("");

        if (!technique)
            return;

        std::ostringstream ss;

        const std::string NA = "NA";
        const std::string endl = "\n";

        std::string author = technique->getAuthor().empty() ? NA : std::string(technique->getAuthor());
        std::string version = technique->getVersion().empty() ? NA : std::string(technique->getVersion());
        std::string description = technique->getDescription().empty() ? NA : std::string(technique->getDescription());

        auto serializeBool = [](bool value) {
            return value ? "#{sYes}" : "#{sNo}";
        };

        const auto flags = technique->getFlags();

        const auto flag_interior = serializeBool (!(flags & fx::Technique::Flag_Disable_Interiors));
        const auto flag_exterior = serializeBool (!(flags & fx::Technique::Flag_Disable_Exteriors));
        const auto flag_underwater = serializeBool(!(flags & fx::Technique::Flag_Disable_Underwater));
        const auto flag_abovewater = serializeBool(!(flags & fx::Technique::Flag_Disable_Abovewater));

        switch (technique->getStatus())
        {
            case fx::Technique::Status::Success:
            case fx::Technique::Status::Uncompiled:
                ss  << "#{fontcolourhtml=header}Author:      #{fontcolourhtml=normal} " << author << endl << endl
                    << "#{fontcolourhtml=header}Version:     #{fontcolourhtml=normal} " << version << endl << endl
                    << "#{fontcolourhtml=header}Description: #{fontcolourhtml=normal} " << description << endl << endl
                    << "#{fontcolourhtml=header}Interiors: #{fontcolourhtml=normal} " << flag_interior
                    << "#{fontcolourhtml=header}   Exteriors: #{fontcolourhtml=normal} " << flag_exterior
                    << "#{fontcolourhtml=header}   Underwater: #{fontcolourhtml=normal} " << flag_underwater
                    << "#{fontcolourhtml=header}   Abovewater: #{fontcolourhtml=normal} " << flag_abovewater;
                break;
            case fx::Technique::Status::File_Not_exists:
                ss  << "#{fontcolourhtml=negative}Shader Error: #{fontcolourhtml=header} <" << std::string(technique->getFileName()) << ">#{fontcolourhtml=normal} not found." << endl << endl
                    << "Ensure the shader file is in a 'Shaders/' sub directory in a data files directory";
                break;
            case fx::Technique::Status::Parse_Error:
                ss  << "#{fontcolourhtml=negative}Shader Compile Error: #{fontcolourhtml=normal} <" << std::string(technique->getName()) << "> failed to compile." << endl << endl
                    << technique->getLastError();
                break;
        }

        mShaderInfo->setCaptionWithReplacing(ss.str());

        if (Settings::ShaderManager::getMode() == Settings::ShaderManager::Mode::Debug)
        {
            if (technique->getUniformMap().size() > 0)
            {
                MyGUI::Button* resetButton = mConfigArea->createWidget<MyGUI::Button>("MW_Button", {0,0,0,24}, MyGUI::Align::Default);
                resetButton->setCaption("Reset all to default");
                resetButton->setTextAlign(MyGUI::Align::Center);
                resetButton->eventMouseButtonClick += MyGUI::newDelegate(this, &PostProcessorHud::notifyResetButtonClicked);
            }

            for (const auto& uniform : technique->getUniformMap())
            {
                if (!uniform->mStatic || uniform->mSamplerType)
                    continue;

                if (!uniform->mHeader.empty())
                    mConfigArea->createWidget<Gui::AutoSizedTextBox>("MW_UniformGroup", {0,0,0,34}, MyGUI::Align::Default)->setCaption(uniform->mHeader);

                fx::Widgets::UniformBase* uwidget = mConfigArea->createWidget<fx::Widgets::UniformBase>("MW_UniformEdit", {0,0,0,22}, MyGUI::Align::Default);
                uwidget->init(uniform);
            }
        }

        layout();
    }

    void PostProcessorHud::updateTechniques()
    {
        if (!isVisible())
            return;

        std::string hint;
        ListWrapper* hintWidget = nullptr;
        if (mInactiveList->getIndexSelected() != MyGUI::ITEM_NONE)
        {
            hint = mInactiveList->getItemNameAt(mInactiveList->getIndexSelected());
            hintWidget = mInactiveList;
        }
        else if (mActiveList->getIndexSelected() != MyGUI::ITEM_NONE)
        {
            hint = mActiveList->getItemNameAt(mActiveList->getIndexSelected());
            hintWidget = mActiveList;
        }

        mInactiveList->removeAllItems();
        mActiveList->removeAllItems();

        auto* processor = MWBase::Environment::get().getWorld()->getPostProcessor();

        for (const auto& [name, _] : processor->getTechniqueMap())
        {
            auto technique = processor->loadTechnique(name);

            if (!technique)
                continue;

            if (!technique->getHidden() && !processor->isTechniqueEnabled(technique) && name.find(mFilter->getCaption()) != std::string::npos)
                mInactiveList->addItem(name, technique);
        }

        for (auto technique : processor->getTechniques())
        {
            if (!technique->getHidden())
                mActiveList->addItem(technique->getName(), technique);
        }

        auto tryFocus = [this](ListWrapper* widget, const std::string& hint)
        {
            size_t index = widget->findItemIndexWith(hint);

            if (index != MyGUI::ITEM_NONE)
            {
                MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(widget);
                select(widget, index);
            }
        };

        if (!mOverrideHint.empty())
        {
            tryFocus(mActiveList, mOverrideHint);
            tryFocus(mInactiveList, mOverrideHint);

            mOverrideHint.clear();
        }
        else if (hintWidget && !hint.empty())
            tryFocus(hintWidget, hint);
    }

    void PostProcessorHud::registerMyGUIComponents()
    {
        MyGUI::FactoryManager& factory = MyGUI::FactoryManager::getInstance();
        factory.registerFactory<fx::Widgets::UniformBase>("Widget");
        factory.registerFactory<fx::Widgets::EditNumberFloat4>("Widget");
        factory.registerFactory<fx::Widgets::EditNumberFloat3>("Widget");
        factory.registerFactory<fx::Widgets::EditNumberFloat2>("Widget");
        factory.registerFactory<fx::Widgets::EditNumberFloat>("Widget");
        factory.registerFactory<fx::Widgets::EditNumberInt>("Widget");
        factory.registerFactory<fx::Widgets::EditBool>("Widget");
        factory.registerFactory<ListWrapper>("Widget");
    }
}
