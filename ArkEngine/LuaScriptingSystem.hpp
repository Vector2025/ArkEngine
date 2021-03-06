#pragma once

#include <sol/sol.hpp>
#include <ark/ecs/System.hpp>
#include <ark/ecs/Component.hpp>
#include <ark/ecs/Meta.hpp>
#include <ark/ecs/DefaultServices.hpp>
#include <ark/ecs/EntityManager.hpp>
#include <ark/ecs/components/Transform.hpp>
#include <ark/core/Engine.hpp>

#include <filesystem>
#include <iostream>
#include <map>

class LuaScriptingSystem;

#define LUA_SCRIPT_UPDATING 0

// holds lua data that acts as a kind of lua-components, LuaComponentsComponent
struct LuaDataComponent {
	std::unordered_map<std::string, sol::table> components;
};

class LuaDataSystem : ark::SystemT<LuaDataSystem> {
	ark::View<LuaDataComponent> view;
public:

	void init() override {
		view = entityManager.view<LuaDataComponent>();
		//sol::state lua;
		// export un fel de add/get
		//lua[""]
	}

	void update() override {

	}
};

class LuaScriptingComponent {
public:
	LuaScriptingComponent() = default;

	void addScript(std::string_view name);

	void removeScript(std::string_view name);

	static void onAdd(LuaScriptingSystem* sys, ark::EntityManager& man, ark::EntityId entity) {
		auto& comp = man.get<LuaScriptingComponent>(entity);
		comp.mEntity = ark::Entity{ entity, man };
		comp.mSystem = sys;
	}

	~LuaScriptingComponent() {
		for (auto& sc : mScripts)
			sc.abandon();
	}

private:
	//void loadScript(int index);
	ark::Entity mEntity;
	//std::vector<std::pair<bool, sol::table>> mScripts;
	std::vector<sol::table> mScripts;
	std::vector<const std::filesystem::path*> mScriptPaths;
	LuaScriptingSystem* mSystem;
	friend class LuaScriptingSystem;
};

ARK_REGISTER_COMPONENT(LuaScriptingComponent, registerServiceDefault<LuaScriptingComponent>()) { return members<LuaScriptingComponent>(); }

namespace fs = std::filesystem;
class LuaScriptingSystem : public ark::SystemT<LuaScriptingSystem> {
	friend class LuaScriptingComponent;
	sol::state lua;
	std::map<std::filesystem::path, std::filesystem::file_time_type> scriptLastWrites;
	fs::path luaPath;
	ark::View<LuaScriptingComponent> view;

public:
	LuaScriptingSystem() = default;

	constexpr static inline bool dynamicLoading = true;

	sol::state* getState() { return &lua; }

	void init() override
	{
		view = entityManager.view<LuaScriptingComponent>();
		luaPath = ark::Resources::resourceFolder + "lua/";
		lua.open_libraries(sol::lib::base);
		lua["getComponent"] = [](sol::table selfScript, std::string_view componentName, sol::this_state luaState) mutable -> sol::table {
			auto mdata = ark::meta::resolve(componentName);
			auto entity = selfScript["entity"].get<ark::Entity>();
			void* pComp = entity.get(mdata->type);
			auto tableFromPtr = mdata->func<sol::table(sol::state_view, void*)>("lua_table_from_pointer");
			return tableFromPtr(luaState, pComp);
		};

		for (auto compType : entityManager.getTypes()) {
			if (auto exportType = ark::meta::resolve(compType)->func<void(sol::state_view)>("export_to_lua"))
				exportType(lua);
		}
	}

	void update() override
	{
#if LUA_SCRIPT_UPDATING
		for (const auto& [path, lastWrite] : scriptLastWrites) {
			const auto realLastWrite = std::filesystem::last_write_time(path);
			if (lastWrite != realLastWrite) {
				scriptLastWrites[path] = realLastWrite;
				for (auto entity : getEntities()) {
					auto& comp = entity.getComponent<LuaScriptingComponent>();
					int index = Util::get_index(comp.mScriptPaths, &path);
					if (index != ArkInvalidIndex) {
						try {
							auto& script = comp.mScripts[index];
							auto res = lua.safe_script_file(path.string());
							if (res.valid()) {
								script = res.get<sol::table>();
								script["bind"](comp.mScripts[index]);
								script["ark_has_errors"] = false;
								std::cout << "ark reloaded LUA script(" << path << ") on entity(" 
									<< entity.getComponent<ark::TagComponent>().name << ")" << '\n';
							}
						}
						catch (sol::error& e) {
							std::cout << "ark error on reloading LUA script(" << path << ") on entity(" 
								<< entity.getComponent<ark::TagComponent>().name << "): " << e.what() << '\n';
						}
					}
				}
			}
		}
#endif
		for (auto& comp : view) {
			for (auto& script : comp.mScripts) {
				if (script["ark_has_errors"])
					continue;
				auto update = sol::protected_function{ script["update"] };
				auto res = update(script, ark::Engine::deltaTime().asSeconds());
				if (!res.valid()) {
					script["ark_has_errors"] = true;
					if(res.get_type() == sol::type::string)
						std::cout << res.get<sol::string_view>() << '\n';
					//	std::cout << "ark error on calling LUA script update on entity (" << entity.getComponent<ark::TagComponent>().name << "): " << e.what() << '\n';
				}
			}
		}
	}
};

inline void LuaScriptingComponent::addScript(std::string_view str)
{
	try {
		auto fileName = mSystem->luaPath.string() + str.data();
		auto result = mSystem->lua.safe_script_file(fileName);
		if (!result.valid()) {
			std::cout << "ark error on adding LUA script(" << str << ") on entity (" 
				<< mEntity.get<ark::TagComponent>().name << "): " << '\n';
			return;
		}
		auto script = result.get<sol::table>();
		script["entity"] = mEntity;
		script["bind"](script);
		script["ark_has_errors"] = false;
		//script["is_active"] = true;
		mScripts.push_back(script);
		auto lastWrite = std::filesystem::last_write_time(fileName);
		auto [it, _] = mSystem->scriptLastWrites.emplace(fileName, lastWrite);
		mScriptPaths.push_back(&it->first);
	}
	catch (sol::error& e) {
		std::cout << "ark error on adding LUA script(" << str << ") on entity (" 
			<< mEntity.get<ark::TagComponent>().name << "): " << e.what() << '\n';
	}
}

inline void LuaScriptingComponent::removeScript(std::string_view name)
{

}

template <typename Type>
void exportTypeToLua(sol::state_view state)
{
	auto mdata = ark::meta::type<Type>();
	auto type = state.new_usertype<Type>(mdata->name);
	ark::meta::doForAllProperties<Type>([&](auto property) {
		using PropType = ark::meta::get_member_type<decltype(property)>;
		if (property.hasPtr()) {
			type.set(property.getName(), property.getPtr());
		}
		else if (property.hasRefFuncPtrs()) {
			auto [get, set] = property.getRefFuncPtrs();
			type.set(property.getName(), sol::property(get, set));
		}
		else if (property.hasValFuncPtrs()) {
			auto [get, set] = property.getValFuncPtrs();
			type.set(property.getName(), sol::property(get, set));
		}
	});
	ark::meta::doForAllFunctions<Type>([&](auto memFunc) {
		using MemFuncType = ark::meta::get_member_type<decltype(memFunc)>;
		if (memFunc.isConst) {
			type.set_function(memFunc.getName(), memFunc.getConstFunPtr());
		}
		else {
			type.set_function(memFunc.getName(), memFunc.getFunPtr());
		}
	});
}

template <typename T>
auto tableFromPointer(sol::state_view state, void* ptr) -> sol::table
{
	return sol::make_object<T*>(state, static_cast<T*>(ptr));
}

