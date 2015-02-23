/**
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Oct 22, 2014
 * License: LGPLv3+, Apache License, or MIT, your choice
 */

#include "common/cs_Storage.h"
#include "services/cs_GeneralService.h"

#if MESHING==1 
#include <protocol/cs_Mesh.h>
#endif

using namespace BLEpp;

// needs to be the same command as defined in the bootloader
#define COMMAND_ENTER_RADIO_BOOTLOADER          1 

GeneralService::GeneralService(Nrf51822BluetoothStack &stack) :
		_stack(&stack),
		_temperatureCharacteristic(NULL), _nameCharacteristic(NULL),
		_deviceTypeCharacteristic(NULL), _roomCharacteristic(NULL),
		_firmwareCharacteristic(NULL) {

	setUUID(UUID(GENERAL_UUID));
	setName("General Service");

	LOGi("Create general service");
	characStatus.reserve(8);

	characStatus.push_back( { "Temperature",
		TEMPERATURE_UUID,
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addTemperatureCharacteristic) });
	characStatus.push_back( { "Change Name",
		CHANGE_NAME_UUID,
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addChangeNameCharacteristic) });
	characStatus.push_back( { "Device Type",
		DEVICE_TYPE_UUID,
		false,
		static_cast<addCharacteristicFunc>(&GeneralService::addDeviceTypeCharacteristic) });
	characStatus.push_back( { "Room",
		ROOM_UUID,
		false,
		static_cast<addCharacteristicFunc>(&GeneralService::addRoomCharacteristic) });
	characStatus.push_back( { "Firmware",
		FIRMWARE_UUID, 
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addFirmwareCharacteristic) });
#if MESHING==1
	characStatus.push_back( { "Mesh",
		MESH_UUID, 
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addMeshCharacteristic) });
#endif
	characStatus.push_back( { "Set configuration",
		SET_CONFIGURATION_UUID,
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addSetConfigurationCharacteristic) });
	characStatus.push_back( { "Select configuration",
		SELECT_CONFIGURATION_UUID,
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addSelectConfigurationCharacteristic) });
	characStatus.push_back( { "Get configuration",
		GET_CONFIGURATION_UUID,
		true,
		static_cast<addCharacteristicFunc>(&GeneralService::addGetConfigurationCharacteristic) });

	Storage::getInstance().getHandle(PS_ID_GENERAL_SERVICE, _storageHandle);
	loadPersistentStorage();
}

void GeneralService::loadPersistentStorage() {
	Storage::getInstance().getStruct(_storageHandle, &_storageStruct, sizeof(_storageStruct));
}

void GeneralService::savePersistentStorage() {
	Storage::getInstance().setStruct(_storageHandle, &_storageStruct, sizeof(_storageStruct));
}

void GeneralService::addTemperatureCharacteristic() {
	_temperatureCharacteristic = new CharacteristicT<int32_t>();
	_temperatureCharacteristic->setUUID(UUID(getUUID(), TEMPERATURE_UUID));
	_temperatureCharacteristic->setName("Temperature");
	_temperatureCharacteristic->setDefaultValue(0);
	_temperatureCharacteristic->setNotifies(true);

	addCharacteristic(_temperatureCharacteristic);
}

void GeneralService::addDeviceTypeCharacteristic() {
	Storage::getString(_storageStruct.device_type, _type, "Unknown");

//	LOGd("create characteristic to read/write device type");
	_deviceTypeCharacteristic = createCharacteristicRef<std::string>();
	(*_deviceTypeCharacteristic)
		.setUUID(UUID(getUUID(), DEVICE_TYPE_UUID))
		.setName("Device Type")
		.setDefaultValue(_type)
		.setWritable(true)
		.onWrite([&](const std::string value) -> void {
			LOGi("set device type to: %s", value.c_str());
			Storage::setString(value, _storageStruct.device_type);
			savePersistentStorage();
		});
}

void GeneralService::addRoomCharacteristic() {
	Storage::getString(_storageStruct.room, _room, "Unknown");

//	LOGd("create characteristic to read/write room");
	_roomCharacteristic = createCharacteristicRef<std::string>();
	(*_roomCharacteristic)
		.setUUID(UUID(getUUID(), ROOM_UUID))
		.setName("Room")
		.setDefaultValue(_room)
		.setWritable(true)
		.onWrite([&](const std::string value) -> void {
			LOGi("set room to: %s", value.c_str());
			Storage::setString(value, _storageStruct.room);
			savePersistentStorage();
		});
}

void GeneralService::addFirmwareCharacteristic() {
	_firmwareCharacteristic = createCharacteristicRef<int32_t>();
	(*_firmwareCharacteristic)
		.setUUID(UUID(getUUID(), FIRMWARE_UUID))
		.setName("Update firmware")
		.setDefaultValue(0)
		.setWritable(true)
		.onWrite([&](const int32_t & value) -> void {
			if (value != 0) {
				LOGi("Update firmware");
				uint8_t err_code;
				err_code = sd_power_gpregret_clr(0xFF);
				APP_ERROR_CHECK(err_code);
				uint32_t cmd = COMMAND_ENTER_RADIO_BOOTLOADER;
				err_code = sd_power_gpregret_set(cmd);
				APP_ERROR_CHECK(err_code);
				sd_nvic_SystemReset();				
			} else {
				LOGi("Update firmware? Write nonzero value");
			}
		});
}

#if MESHING==1
void GeneralService::addMeshCharacteristic() {
	_meshCharacteristic = createCharacteristicRef<MeshMessage>();
	(*_meshCharacteristic)
		.setUUID(UUID(getUUID(), MESH_UUID))
		.setName("Mesh")
		.setWritable(true)
		.onWrite([&](const MeshMessage & value) -> void {
			LOGi("Send mesh message");
			//uint8_t id = value.id(); // not used
			uint8_t handle = value.handle();
			uint8_t val = value.value();
			CMesh &mesh = CMesh::getInstance();
			mesh.send(handle, val);
		});
}
#endif

void GeneralService::addSetConfigurationCharacteristic() {
	_setConfigurationCharacteristic = createCharacteristicRef<StreamBuffer>();
	(*_setConfigurationCharacteristic)
		.setUUID(UUID(getUUID(), SET_CONFIGURATION_UUID))
		.setName("Set Configuration")
		.setWritable(true)
		.onWrite([&](const StreamBuffer& value) -> void {
			LOGi("Write configuration value");
			uint8_t type = value.type();
			switch(type) {
			case CONFIG_NAME_UUID: {
				LOGd("Write name");
				std::string str = std::string((char*)value.payload());
				setBLEName(str);
				Storage::setString(str, _storageStruct.device_name);
				savePersistentStorage();
			}
			case CONFIG_FLOOR_UUID: {
				LOGd("Set floor level");
				if (value.length() != 1) {
					LOGw("We do not account for buildings of more than 255 floors yet");
					return;
				}
				uint8_t floor = value.payload()[0];
				LOGi("Set floor to %i", floor);
				Storage::setUint8(floor, _storageStruct.floor);
			} default: 
				LOGw("There is no such configuration id!");
			}
				
		});
}

void GeneralService::addSelectConfigurationCharacteristic() {
	_selectConfigurationCharacteristic = createCharacteristicRef<uint8_t>();
	(*_selectConfigurationCharacteristic)
		.setUUID(UUID(getUUID(), SELECT_CONFIGURATION_UUID))
		.setName("Select Configuration")
		.setWritable(true) 
		.onWrite([&](const uint8_t& value) -> void {
			if (value < CONFIG_TYPES) {
				_selectConfiguration = value;
			} else {
				LOGe("Cannot select %i", value);
			}
		});
}

void GeneralService::addGetConfigurationCharacteristic() {
	_getConfigurationCharacteristic = createCharacteristicRef<StreamBuffer>();
	(*_getConfigurationCharacteristic)
		.setUUID(UUID(getUUID(), GET_CONFIGURATION_UUID))
		.setName("Get Configuration")
		.setWritable(false);
}

void GeneralService::addChangeNameCharacteristic() {
	Storage::getString(_storageStruct.device_name, _name, getBLEName());
	setBLEName(_name);

	_nameCharacteristic = createCharacteristicRef<std::string>();
	(*_nameCharacteristic)
		.setUUID(UUID(getUUID(), CHANGE_NAME_UUID))
		.setName("Change Name")
		.setDefaultValue(_name)
		.setWritable(true)
		.onWrite([&](const std::string& value) -> void {
			LOGi("Set bluetooth name to: %s", value.c_str());
			setBLEName(value);
			Storage::setString(value, _storageStruct.device_name);
			savePersistentStorage();
		});
}

std::string & GeneralService::getBLEName() {
	_name = "Unknown";
	if (_stack) {
		return _name = _stack->getDeviceName();
	}
	return _name;
}

void GeneralService::setBLEName(const std::string &name) {
	if (name.length() > 31) {
		log(ERROR, "Name is too long");
		return;
	}
	if (_stack) {
		_stack->updateDeviceName(name);
	}
}

GeneralService& GeneralService::createService(Nrf51822BluetoothStack& stack) {
	GeneralService* svc = new GeneralService(stack);
	stack.addService(svc);
	svc->GenericService::addSpecificCharacteristics();
	return *svc;
}

void GeneralService::setTemperature(int32_t temperature) {
	*_temperatureCharacteristic = temperature;
}

void GeneralService::tick() {
	if (_temperatureCharacteristic) {
		int32_t temp;
		temp = getTemperature();
		setTemperature(temp);
	}
}