#pragma once

#include "ark/ecs/SceneInspector.hpp"
#include "ark/ecs/SerdeJsonDirector.hpp"

#define ARK_SERVICE_INSPECTOR ark::meta::service(ark::SceneInspector::serviceName, &ark::SceneInspector::renderPropertiesOfType<Type>)

#define ARK_SERVICE_SERDE \
	ark::meta::service(ark::SerdeJsonDirector::serviceSerializeName, ark::serialize_value<Type>), \
	ark::meta::service(ark::SerdeJsonDirector::serviceDeserializeName, ark::deserialize_value<Type>)

#define ARK_DEFAULT_SERVICES \
	ARK_SERVICE_INSPECTOR, ARK_SERVICE_SERDE
