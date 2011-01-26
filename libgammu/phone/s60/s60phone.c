/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (c) 2011 Michal Cihar <michal@cihar.com>
 */


#include "../../gsmstate.h"
#include "../../protocol/s60/s60-ids.h"
#include "../../gsmcomon.h"
#include "../../misc/coding/coding.h"
#include "../../gsmphones.h"
#include "../../gsmstate.h"
#include "../../service/gsmmisc.h"
#include "../pfunc.h"
#include <string.h>

#if defined(GSM_ENABLE_S60)
GSM_Error S60_Initialise(GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	size_t i;

	Priv->ContactLocations = NULL;
	Priv->ContactLocationsSize = 0;
	Priv->ContactLocationsPos = 0;

	Priv->CalendarLocations = NULL;
	Priv->CalendarLocationsSize = 0;
	Priv->CalendarLocationsPos = 0;

	Priv->ToDoLocations = NULL;
	Priv->ToDoLocationsSize = 0;
	Priv->ToDoLocationsPos = 0;

	s->Phone.Data.NetworkInfo = NULL;
	s->Phone.Data.SignalQuality = NULL;
	s->Phone.Data.BatteryCharge = NULL;
	s->Phone.Data.Memory = NULL;
	s->Phone.Data.MemoryStatus = NULL;
	s->Phone.Data.CalStatus = NULL;
	s->Phone.Data.ToDoStatus = NULL;

	for (i = 0; i < sizeof(Priv->MessageParts) / sizeof(Priv->MessageParts[0]); i++) {
		Priv->MessageParts[i] = NULL;
	}

	error = GSM_WaitFor (s, NULL, 0, 0, S60_TIMEOUT, ID_Initialise);
	if (error != ERR_NONE) {
		return error;
	}

	if (Priv->MajorVersion != 1 || Priv->MinorVersion != 5) {
		smprintf(s, "Unsupported protocol version\n");
		return ERR_NOTSUPPORTED;
	}

	error = GSM_WaitFor(s, NULL, 0, NUM_HELLO_REQUEST, S60_TIMEOUT, ID_EnableEcho);
	if (error != ERR_NONE) {
		return error;
	}

	return error;

//	return ERR_NONE;
}

/**
 * Splits values from S60 reply.
 */
GSM_Error S60_SplitValues(GSM_Protocol_Message *msg, GSM_StateMachine *s)
{
	unsigned char * pos = msg->Buffer - 1;
	size_t i;
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	for (i = 0; i < sizeof(Priv->MessageParts) / sizeof(Priv->MessageParts[0]); i++) {
		Priv->MessageParts[i] = NULL;
	}

	i = 0;

	while ((pos - msg->Buffer) < (ssize_t)msg->Length) {
		if (i >  sizeof(Priv->MessageParts) / sizeof(Priv->MessageParts[0])) {
			smprintf(s, "Too many reply parts!\n");
			return ERR_MOREMEMORY;
		}
		Priv->MessageParts[i++] = pos + 1;

		/* Find end of next field */
		pos = strchr(pos + 1, NUM_SEPERATOR);
		if (pos == NULL) {
			break;
		}

		/* Zero terminate string */
		*pos = 0;
	}
	return ERR_NONE;
}

GSM_Error S60_Terminate(GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	free(Priv->ContactLocations);
	Priv->ContactLocations = NULL;
	Priv->ContactLocationsSize = 0;
	Priv->ContactLocationsPos = 0;

	free(Priv->CalendarLocations);
	Priv->CalendarLocations = NULL;
	Priv->CalendarLocationsSize = 0;
	Priv->CalendarLocationsPos = 0;

	free(Priv->ToDoLocations);
	Priv->ToDoLocations = NULL;
	Priv->ToDoLocationsSize = 0;
	Priv->ToDoLocationsPos = 0;

	return GSM_WaitFor(s, NULL, 0, NUM_QUIT, S60_TIMEOUT, ID_Terminate);
}

static GSM_Error S60_Reply_Generic(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	switch (msg.Type) {
		case NUM_SYSINFO_REPLY_START:
		case NUM_CONTACTS_REPLY_HASH_SINGLE_START:
		case NUM_CONTACTS_REPLY_CONTACT_START:
		case NUM_CALENDAR_REPLY_ENTRIES_START:
			return ERR_NEEDANOTHERANSWER;
		case NUM_LOCATION_REPLY_NA:
			return ERR_NOTSUPPORTED;
		case NUM_CONTACTS_REPLY_CONTACT_NOT_FOUND:
		case NUM_CALENDAR_REPLY_ENTRY_NOT_FOUND:
			return ERR_EMPTY;
		default:
			return ERR_NONE;
	}
}


static GSM_Error S60_Reply_Connect(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	char *pos;

	Priv->MajorVersion = atoi(msg.Buffer);
	pos = strchr(msg.Buffer, '.');
	if (pos == NULL) {
		return ERR_UNKNOWN;
	}
	Priv->MinorVersion = atoi(pos + 1);
	smprintf(s, "Connected to series60-remote version %d.%d\n", Priv->MajorVersion, Priv->MinorVersion);

	return ERR_NONE;
}

static GSM_Error S60_GetInfo(GSM_StateMachine *s)
{
	return GSM_WaitFor(s, "1", 1, NUM_SYSINFO_REQUEST, S60_TIMEOUT, ID_GetModel);
}

static GSM_Error S60_GetSignalQuality(GSM_StateMachine *s, GSM_SignalQuality *sig)
{
	GSM_Error error;

	sig->BitErrorRate = -1;
	sig->SignalStrength = -1;
	sig->SignalPercent = -1;

	s->Phone.Data.SignalQuality = sig;
	error = S60_GetInfo(s);
	s->Phone.Data.SignalQuality = NULL;
	return error;
}

static GSM_Error S60_GetNetworkInfo(GSM_StateMachine *s, GSM_NetworkInfo *netinfo)
{
	GSM_Error error;

	s->Phone.Data.NetworkInfo = netinfo;
	error = GSM_WaitFor(s, NULL, 0, NUM_LOCATION_REQUEST, S60_TIMEOUT, ID_GetNetworkInfo);
	s->Phone.Data.NetworkInfo = NULL;
	return error;
}

static GSM_Error S60_Reply_GetNetworkInfo(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	char *mcc, *mnc, *lac, *cellid;

	if (s->Phone.Data.NetworkInfo == NULL) {
		return ERR_NONE;
	}

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}

	/* Grab values */
	mcc = Priv->MessageParts[0];
	mnc = Priv->MessageParts[1];
	lac = Priv->MessageParts[2];
	cellid =Priv->MessageParts[3];

	/* We need all of them */
	if (mcc == NULL || mnc == NULL || lac == NULL || cellid == NULL) {
		return ERR_UNKNOWN;
	}

	strcpy(s->Phone.Data.NetworkInfo->CID, cellid);
	strcpy(s->Phone.Data.NetworkInfo->NetworkCode, mcc);
	strcat(s->Phone.Data.NetworkInfo->NetworkCode, " ");
	strcat(s->Phone.Data.NetworkInfo->NetworkCode, mnc);
	s->Phone.Data.NetworkInfo->State = GSM_NetworkStatusUnknown;
	strcpy(s->Phone.Data.NetworkInfo->LAC, lac);
	s->Phone.Data.NetworkInfo->NetworkName[0] = 0;
	s->Phone.Data.NetworkInfo->NetworkName[1] = 0;
	s->Phone.Data.NetworkInfo->GPRS = 0;
	s->Phone.Data.NetworkInfo->PacketCID[0] = 0;
	s->Phone.Data.NetworkInfo->PacketState = 0;
	s->Phone.Data.NetworkInfo->PacketLAC[0] = 0;

	return ERR_NONE;
}

static GSM_Error S60_GetBatteryCharge(GSM_StateMachine *s, GSM_BatteryCharge *bat)
{
	GSM_Error error;

	GSM_ClearBatteryCharge(bat);
	s->Phone.Data.BatteryCharge = bat;
	error = S60_GetInfo(s);
	s->Phone.Data.BatteryCharge = NULL;
	return error;
}


static GSM_Error S60_Reply_GetInfo(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	char *pos;
	GSM_SignalQuality *Signal = s->Phone.Data.SignalQuality;
	GSM_BatteryCharge *BatteryCharge = s->Phone.Data.BatteryCharge;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}
	if (Priv->MessageParts[0] == NULL || Priv->MessageParts[1] == NULL) {
		return ERR_UNKNOWN;
	}
	smprintf(s, "Received %s=%s\n", Priv->MessageParts[0], Priv->MessageParts[1]);
	if (strcmp(Priv->MessageParts[0], "imei") == 0) {
		strcpy(s->Phone.Data.IMEI, Priv->MessageParts[1]);
	} else if (strcmp(Priv->MessageParts[0], "model") == 0) {
		/* Parse manufacturer */
		pos = strstr(Priv->MessageParts[1], "(C)");
		if (pos != NULL) {
			strcpy(s->Phone.Data.Manufacturer, pos + 3);
		}
		/* Try to find model */
		pos = strchr(Priv->MessageParts[1], ' ');
		if (pos != NULL) {
			pos = strchr(pos + 1, ' ');
			if (pos != NULL) {
				strcpy(s->Phone.Data.Model, pos + 1);
				pos = strchr(s->Phone.Data.Model, ' ');
				if (pos != NULL) {
					*pos = 0;
				}
			} else {
				strcpy(s->Phone.Data.Model, Priv->MessageParts[1]);
			}
		} else {
			strcpy(s->Phone.Data.Model, Priv->MessageParts[1]);
		}
		s->Phone.Data.ModelInfo = GetModelData(s, NULL, s->Phone.Data.Model, NULL);

		if (s->Phone.Data.ModelInfo->number[0] == 0)
			s->Phone.Data.ModelInfo = GetModelData(s, NULL, NULL, s->Phone.Data.Model);

		if (s->Phone.Data.ModelInfo->number[0] == 0)
			s->Phone.Data.ModelInfo = GetModelData(s, s->Phone.Data.Model, NULL, NULL);

		if (s->Phone.Data.ModelInfo->number[0] == 0) {
			smprintf(s, "Unknown model, but it should still work\n");
		}
		smprintf(s, "[Model name: `%s']\n", s->Phone.Data.Model);
		smprintf(s, "[Model data: `%s']\n", s->Phone.Data.ModelInfo->number);
		smprintf(s, "[Model data: `%s']\n", s->Phone.Data.ModelInfo->model);
	} else if (strcmp(Priv->MessageParts[0], "s60_version") == 0) {
		strcpy(s->Phone.Data.Version, Priv->MessageParts[1]);
		strcat(s->Phone.Data.Version, ".");
		strcat(s->Phone.Data.Version, Priv->MessageParts[2]);
		GSM_CreateFirmwareNumber(s);
	} else if (Signal != NULL && strcmp(Priv->MessageParts[0], "signal_dbm") == 0) {
		Signal->SignalStrength = atoi(Priv->MessageParts[1]);
	} else if (Signal != NULL && strcmp(Priv->MessageParts[0], "signal_bars") == 0) {
		Signal->SignalPercent = 100 * 7 / atoi(Priv->MessageParts[1]);
	} else if (BatteryCharge != NULL && strcmp(Priv->MessageParts[0], "battery") == 0) {
		BatteryCharge->BatteryPercent = atoi(Priv->MessageParts[1]);
	}
	return ERR_NEEDANOTHERANSWER;
}

static GSM_Error S60_GetMemoryStatus(GSM_StateMachine *s, GSM_MemoryStatus *Status)
{
	GSM_Error error;

	if (Status->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}

	s->Phone.Data.MemoryStatus = Status;
	Status->MemoryUsed = 0;
	Status->MemoryFree = 1000;
	error = GSM_WaitFor(s, "", 0, NUM_CONTACTS_REQUEST_COUNT, S60_TIMEOUT, ID_GetMemoryStatus);
	s->Phone.Data.MemoryStatus = NULL;
	return error;
}

static GSM_Error S60_GetMemoryLocations(GSM_StateMachine *s)
{
	return GSM_WaitFor(s, "", 0, NUM_CONTACTS_REQUEST_HASH_SINGLE, S60_TIMEOUT, ID_GetMemoryStatus);
}

static GSM_Error S60_StoreLocation(GSM_StateMachine *s, int **locations, size_t *size, size_t *pos, int location)
{
	if ((*pos) + 3 >= (*size)) {
		*locations = (int *)realloc(*locations, ((*size) + 20) * sizeof(int));
		if (*locations == NULL) {
			return ERR_MOREMEMORY;
		}
		*size += 20;
	}
	(*locations)[(*pos)] = location;
	(*locations)[(*pos) + 1] = 0;
	(*pos) += 1;
	return ERR_NONE;
}


static GSM_Error S60_Reply_GetMemoryStatus(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	s->Phone.Data.MemoryStatus->MemoryUsed = atoi(msg.Buffer);

	return ERR_NONE;
}

static GSM_Error S60_Reply_ContactHash(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;

	}

	if (Priv->MessageParts[0] == NULL) {
		return ERR_UNKNOWN;
	}

	error = S60_StoreLocation(s, &Priv->ContactLocations, &Priv->ContactLocationsSize, &Priv->ContactLocationsPos, atoi(Priv->MessageParts[0]));
	if (error != ERR_NONE) {
		return error;

	}

	return ERR_NEEDANOTHERANSWER;
}

static GSM_Error S60_GetCalendarStatus(GSM_StateMachine *s, GSM_CalendarStatus *Status)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;

	s->Phone.Data.CalStatus = Status;
	Status->Used = 0;
	Status->Free = 1000;
	Priv->CalendarLocationsPos = 0;
	error = GSM_WaitFor(s, "", 0, NUM_CALENDAR_REQUEST_ENTRIES_ALL, S60_TIMEOUT, ID_GetCalendarNotesInfo);
	s->Phone.Data.CalStatus = NULL;
	return error;
}

static GSM_Error S60_Reply_CalendarCount(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;

	}

	if (Priv->MessageParts[0] == NULL || Priv->MessageParts[1] == NULL) {
		return ERR_UNKNOWN;
	}

	if (strcmp(Priv->MessageParts[1], "appointment") != 0 &&
		strcmp(Priv->MessageParts[1], "event") != 0 &&
		strcmp(Priv->MessageParts[1], "anniversary") != 0) {
		return ERR_NEEDANOTHERANSWER;
	}

	error = S60_StoreLocation(s, &Priv->CalendarLocations, &Priv->CalendarLocationsSize, &Priv->CalendarLocationsPos, atoi(Priv->MessageParts[0]));
	if (error != ERR_NONE) {
		return error;

	}

	if (s->Phone.Data.CalStatus != NULL) {
		s->Phone.Data.CalStatus->Used++;
	}
	return ERR_NEEDANOTHERANSWER;
}

static GSM_Error S60_GetToDoStatus(GSM_StateMachine *s, GSM_ToDoStatus *Status)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;

	s->Phone.Data.ToDoStatus = Status;
	Status->Used = 0;
	Status->Free = 1000;
	Priv->ToDoLocationsPos = 0;
	error = GSM_WaitFor(s, "", 0, NUM_CALENDAR_REQUEST_ENTRIES_ALL, S60_TIMEOUT, ID_GetToDoInfo);
	s->Phone.Data.ToDoStatus = NULL;
	return error;
}

static GSM_Error S60_Reply_ToDoCount(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;

	}

	if (Priv->MessageParts[0] == NULL || Priv->MessageParts[1] == NULL) {
		return ERR_UNKNOWN;
	}

	if (strcmp(Priv->MessageParts[1], "todo") != 0) {
		return ERR_NEEDANOTHERANSWER;
	}

	error = S60_StoreLocation(s, &Priv->ToDoLocations, &Priv->ToDoLocationsSize, &Priv->ToDoLocationsPos, atoi(Priv->MessageParts[0]));
	if (error != ERR_NONE) {
		return error;

	}

	if (s->Phone.Data.ToDoStatus != NULL) {
		s->Phone.Data.ToDoStatus->Used++;
	}
	return ERR_NEEDANOTHERANSWER;
}

GSM_Error S60_GetMemory(GSM_StateMachine *s, GSM_MemoryEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	if (Entry->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}
	Entry->EntriesNum = 0;

	sprintf(buffer, "%d", Entry->Location);

	s->Phone.Data.Memory = Entry;
	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CONTACTS_REQUEST_CONTACT, S60_TIMEOUT, ID_GetMemory);
	s->Phone.Data.Memory = NULL;

	return error;
}

GSM_Error S60_GetNextMemory(GSM_StateMachine *s, GSM_MemoryEntry *Entry, gboolean Start)
{
	GSM_Error error;
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	if (Entry->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}

	if (Start) {
		error = S60_GetMemoryLocations(s);
		if (error != ERR_NONE) {
			return error;
		}
		Priv->ContactLocationsPos = 0;
	}

	if (Priv->ContactLocations[Priv->ContactLocationsPos] == 0) {
		return ERR_EMPTY;
	}

	Entry->Location = Priv->ContactLocations[Priv->ContactLocationsPos++];

	return S60_GetMemory(s, Entry);
}

static GSM_Error S60_Reply_GetMemory(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	char *pos, *type, *location, *value;
	GSM_MemoryEntry *Entry;
	gboolean text = FALSE;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}

	Entry = s->Phone.Data.Memory;

	/* Grab values */
	pos = Priv->MessageParts[0];
	type = Priv->MessageParts[1];
	location = Priv->MessageParts[2];
	value =Priv->MessageParts[3];

	/* We need all of them */
	if (pos == NULL || type == NULL || location == NULL || value == NULL) {
		return ERR_UNKNOWN;
	}

	/* Handle location */
	if ((strcmp(location, "work") == 0)) {
		Entry->Entries[Entry->EntriesNum].Location = PBK_Location_Work;
	} else if ((strcmp(location, "home") == 0)) {
		Entry->Entries[Entry->EntriesNum].Location = PBK_Location_Home;
	} else {
		Entry->Entries[Entry->EntriesNum].Location = PBK_Location_Unknown;
	}

	/* Store in contacts */
	if(strcmp(type, "city") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_City;
	} else if(strcmp(type, "company_name") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Company;
	} else if(strcmp(type, "country") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Country;
	} else if(strcmp(type, "date") == 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Date;
		if (!ReadVCALDateTime(value, &(Entry->Entries[Entry->EntriesNum].Date))) {
			return ERR_UNKNOWN;
		}
	} else if(strcmp(type, "dtmf_string") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_DTMF;
	} else if(strcmp(type, "email_address") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Email;
	} else if(strcmp(type, "extended_address") == 0) {
		text = TRUE;
	} else if(strcmp(type, "fax_number") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Number_Fax;
	} else if(strcmp(type, "first_name") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_FirstName;
	} else if(strcmp(type, "second_name") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_SecondName;
	} else if(strcmp(type, "job_title") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_JobTitle;
	} else if(strcmp(type, "last_name") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_LastName;
	} else if(strcmp(type, "voip") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_VOIP;
	} else if(strcmp(type, "sip_id") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_SIP;
	} else if(strcmp(type, "push_to_talk") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_PushToTalkID;
	} else if(strcmp(type, "mobile_number") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Number_Mobile;
	} else if(strcmp(type, "note") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Note;
	} else if(strcmp(type, "pager_number") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Number_Pager;
	} else if(strcmp(type, "phone_number") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Number_General;
	} else if(strcmp(type, "postal_address") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Postal;
	} else if(strcmp(type, "postal_code") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_Zip;
	} else if(strcmp(type, "state") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_State;
	} else if(strcmp(type, "street_address") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_StreetAddress;
	} else if(strcmp(type, "url") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_URL;
	} else if(strcmp(type, "video_number") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Number_Video;
	} else if(strcmp(type, "wvid") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_WVID;
	} else if(strcmp(type, "thumbnail_image") == 0) {
		/* TODO */
	} else if(strcmp(type, "suffix") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_NameSuffix;
	} else if(strcmp(type, "prefix") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_NamePrefix;
	} else if(strcmp(type, "share_view") == 0) {
		text = TRUE;
		Entry->Entries[Entry->EntriesNum].EntryType = PBK_Text_SWIS;
	} else {
		smprintf(s, "WARNING: Ignoring unknown field type: %s\n", type);
		return ERR_NEEDANOTHERANSWER;
	}

	if (text) {
		DecodeUTF8(Entry->Entries[Entry->EntriesNum].Text, value, strlen(value));
	}

	Entry->EntriesNum++;

	return ERR_NEEDANOTHERANSWER;
}

GSM_Error S60_DeleteMemory(GSM_StateMachine *s, GSM_MemoryEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	if (Entry->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}

	sprintf(buffer, "%d", Entry->Location);

	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CONTACTS_DELETE, S60_TIMEOUT, ID_None);

	return error;
}

GSM_Error S60_SetMemoryEntry(GSM_StateMachine *s, GSM_SubMemoryEntry *Entry, int pos, int reqtype)
{
	const char *type, *location = "none";
	char value[(GSM_PHONEBOOK_TEXT_LENGTH + 1) * 2];
	char buffer [100 + (GSM_PHONEBOOK_TEXT_LENGTH + 1) * 2];
	gboolean text = FALSE;

	switch (Entry->Location) {
		case PBK_Location_Unknown:
			location = "none";
			break;
		case PBK_Location_Home:
			location = "home";
			break;
		case PBK_Location_Work:
			location = "work";
			break;
	}

	switch (Entry->EntryType) {
		case PBK_Text_City:
			type = "city";
			text = TRUE;
			break;
		case PBK_Text_Company:
			type = "company_name";
			text = TRUE;
			break;
		case PBK_Text_Country:
			type = "country";
			text = TRUE;
			break;
		case PBK_Date:
			type = "date";
			snprintf(value, sizeof(value), "%04d%02d%02d", Entry->Date.Year, Entry->Date.Month, Entry->Date.Day);
			break;
		case PBK_Text_DTMF:
			type = "dtmf_string";
			text = TRUE;
			break;
		case PBK_Text_Email:
			type = "email_address";
			text = TRUE;
			break;
		case PBK_Number_Fax:
			type = "fax_number";
			text = TRUE;
			break;
		case PBK_Text_FirstName:
			type = "first_name";
			text = TRUE;
			break;
		case PBK_Text_SecondName:
			type = "second_name";
			text = TRUE;
			break;
		case PBK_Text_JobTitle:
			type = "job_title";
			text = TRUE;
			break;
		case PBK_Text_LastName:
			type = "last_name";
			text = TRUE;
			break;
		case PBK_Text_VOIP:
			type = "voip";
			text = TRUE;
			break;
		case PBK_Text_SIP:
			type = "sip_id";
			text = TRUE;
			break;
		case PBK_PushToTalkID:
			type = "push_to_talk";
			text = TRUE;
			break;
		case PBK_Number_Mobile:
			type = "mobile_number";
			text = TRUE;
			break;
		case PBK_Text_Note:
			type = "note";
			text = TRUE;
			break;
		case PBK_Number_Pager:
			type = "pager";
			text = TRUE;
			break;
		case PBK_Number_General:
			type = "phone_number";
			text = TRUE;
			break;
		case PBK_Text_Postal:
			type = "postal_address";
			text = TRUE;
			break;
		case PBK_Text_Zip:
			type = "postal_code";
			text = TRUE;
			break;
		case PBK_Text_State:
			type = "state";
			text = TRUE;
			break;
		case PBK_Text_StreetAddress:
			type = "street_address";
			text = TRUE;
			break;
		case PBK_Text_URL:
			type = "url";
			text = TRUE;
			break;
		case PBK_Number_Video:
			type = "video_number";
			text = TRUE;
			break;
		case PBK_Text_WVID:
			type = "wvid";
			text = TRUE;
			break;
		case PBK_Text_NameSuffix:
			type = "suffix";
			text = TRUE;
			break;
		case PBK_Text_NamePrefix:
			type = "prefix";
			text = TRUE;
			break;
		case PBK_Text_SWIS:
			type = "share_view";
			text = TRUE;
			break;
		default:
			Entry->AddError = ERR_NOTIMPLEMENTED;
			return ERR_NONE;
	}

	if (text) {
		EncodeUTF8(value, Entry->Text);
	}

	snprintf(buffer, sizeof(buffer), "%d%c%s%c%s%c%s%c",
		pos, NUM_SEPERATOR,
		type , NUM_SEPERATOR,
		location, NUM_SEPERATOR,
		value, NUM_SEPERATOR);

	return GSM_WaitFor(s, buffer, strlen(buffer), reqtype, S60_TIMEOUT, ID_None);
}

GSM_Error S60_SetMemory(GSM_StateMachine *s, GSM_MemoryEntry *Entry)
{
	GSM_MemoryEntry oldentry;
	GSM_Error error;
	int i;

	if (Entry->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}
	oldentry.MemoryType = Entry->MemoryType;
	oldentry.Location = Entry->Location;

	/* Read existing entry */
	error = S60_GetMemory(s, &oldentry);
	if (error != ERR_NONE) {
		return error;
	}

	/* TODO: Here should be some clever method for doing only needed changes */

	/* Delete all existing fields */
	for (i = 0; i < oldentry.EntriesNum; i++) {
		error = S60_SetMemoryEntry(s, &(oldentry.Entries[i]), Entry->Location, NUM_CONTACTS_CHANGE_REMOVEFIELD);
		if (error != ERR_NONE) {
			return error;
		}
	}

	/* Set all new fields */
	for (i = 0; i < Entry->EntriesNum; i++) {
		error = S60_SetMemoryEntry(s, &(Entry->Entries[i]), Entry->Location, NUM_CONTACTS_CHANGE_ADDFIELD);
		if (error != ERR_NONE) {
			return error;
		}
	}

	return ERR_NONE;
}

GSM_Error S60_AddMemory(GSM_StateMachine *s, GSM_MemoryEntry *Entry)
{
	GSM_Error error;

	if (Entry->MemoryType != MEM_ME) {
		return ERR_NOTSUPPORTED;
	}

	s->Phone.Data.Memory = Entry;
	error = GSM_WaitFor(s, NULL, 0, NUM_CONTACTS_ADD, S60_TIMEOUT, ID_SetMemory);
	s->Phone.Data.Memory = NULL;

	if (error != ERR_NONE) {
		return error;
	}

	return S60_SetMemory(s, Entry);
}

static GSM_Error S60_Reply_AddMemory(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	s->Phone.Data.Memory->Location = atoi(msg.Buffer);
	smprintf(s, "Added contact ID %d\n", s->Phone.Data.Memory->Location);
	return ERR_NONE;
}

static GSM_Error S60_Reply_GetCalendar(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	char *pos, *type, *content, *location, *start, *end, *modified, *replication, *alarm_time, *priority, *repeat, *repeat_rule, *repeat_exceptions, *repeat_start, *repeat_end, *interval;
	GSM_CalendarEntry *Entry;
	int i;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}

	/* Check for required fields */
	for (i = 0; i < 16; i++) {
		if (Priv->MessageParts[i] == NULL) {
			smprintf(s, "Not enough parts in reply!\n");
			return ERR_UNKNOWN;
		}
	}

	Entry = s->Phone.Data.Cal;

	/* Grab values */
	pos = Priv->MessageParts[0];
	type = Priv->MessageParts[1];
	content = Priv->MessageParts[2];
	location = Priv->MessageParts[3];
	start = Priv->MessageParts[4];
	end = Priv->MessageParts[5];
	modified = Priv->MessageParts[6];
	replication = Priv->MessageParts[7];
	alarm_time = Priv->MessageParts[8];
	priority = Priv->MessageParts[9];
	repeat = Priv->MessageParts[10];
	repeat_rule = Priv->MessageParts[11];
	repeat_exceptions = Priv->MessageParts[12];
	repeat_start = Priv->MessageParts[13];
	repeat_end = Priv->MessageParts[14];
	interval = Priv->MessageParts[15];

	/* Check for correct type */
	if (strcmp(type, "appointment") == 0) {
		Entry->Type = GSM_CAL_REMINDER;
	} else if (strcmp(type, "event") == 0) {
		Entry->Type = GSM_CAL_MEMO;
	} else if (strcmp(type, "anniversary") == 0) {
		Entry->Type = GSM_CAL_BIRTHDAY;
	} else {
		return ERR_EMPTY;
	}

	if (strlen(content) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_TEXT;
		DecodeUTF8(Entry->Entries[Entry->EntriesNum].Text, content, strlen(content));
		Entry->EntriesNum++;
	}

	if (strlen(location) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_LOCATION;
		DecodeUTF8(Entry->Entries[Entry->EntriesNum].Text, location, strlen(location));
		Entry->EntriesNum++;
	}

	if (strlen(start) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_START_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), start);
		Entry->EntriesNum++;
	}

	if (strlen(end) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_END_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), end);
		Entry->EntriesNum++;
	}

	if (strlen(modified) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_LAST_MODIFIED;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), modified);
		Entry->EntriesNum++;
	}

	if (strlen(replication) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_PRIVATE;
		if (strcmp(replication, "open") == 0) {
			Entry->Entries[Entry->EntriesNum].Number = 0;
		} else {
			Entry->Entries[Entry->EntriesNum].Number = 1;
		}
		Entry->EntriesNum++;
	}

	if (strlen(alarm_time) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_TONE_ALARM_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), alarm_time);
		Entry->EntriesNum++;
	}

	if ((strlen(repeat) > 0) && (strlen(repeat_rule) > 0)) {
		if (strcmp(repeat, "daily") == 0 ) {
		} else if (strcmp(repeat, "weekly") == 0 ) {
			Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_DAYOFWEEK;
			Entry->Entries[Entry->EntriesNum].Number = atoi(repeat_rule);
			Entry->EntriesNum++;
		} else if (strcmp(repeat, "monthly_by_dates") == 0 ) {
			Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_DAY;
			Entry->Entries[Entry->EntriesNum].Number = atoi(repeat_rule);
			Entry->EntriesNum++;
		} else if (strcmp(repeat, "monthly_by_days") == 0 ) {
		} else if (strcmp(repeat, "yearly_by_date") == 0 ) {
		} else if (strcmp(repeat, "yearly_by_day") == 0 ) {
			Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_DAYOFYEAR;
			Entry->Entries[Entry->EntriesNum].Number = atoi(repeat_rule);
			Entry->EntriesNum++;
		} else {
			smprintf(s, "Unknown value for repeating: %s\n", repeat);
			return ERR_UNKNOWN;
		}
	}

	if (strlen(repeat_start) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_STARTDATE;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), repeat_start);
		Entry->EntriesNum++;
	}

	if (strlen(repeat_end) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_STOPDATE;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), repeat_end);
		Entry->EntriesNum++;
	}

	if (strlen(interval) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = CAL_REPEAT_FREQUENCY;
		Entry->Entries[Entry->EntriesNum].Number = atoi(interval);
		Entry->EntriesNum++;
	}


	/* TODO: implement rest (priority, repeating) */

	return ERR_NONE;
}

GSM_Error S60_GetCalendar(GSM_StateMachine *s, GSM_CalendarEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	Entry->EntriesNum = 0;

	sprintf(buffer, "%d", Entry->Location);

	s->Phone.Data.Cal = Entry;
	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CALENDAR_REQUEST_ENTRY, S60_TIMEOUT, ID_GetCalendarNote);
	s->Phone.Data.Cal = NULL;

	return error;
}

GSM_Error S60_GetNextCalendar(GSM_StateMachine *s, GSM_CalendarEntry *Entry, gboolean Start)
{
	GSM_Error error;
	GSM_CalendarStatus Status;
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	if (Start) {
		error = S60_GetCalendarStatus(s, &Status);
		if (error != ERR_NONE) {
			return error;
		}
		Priv->CalendarLocationsPos = 0;
	}

	if (Priv->CalendarLocations[Priv->CalendarLocationsPos] == 0) {
		return ERR_EMPTY;
	}

	Entry->Location = Priv->CalendarLocations[Priv->CalendarLocationsPos++];

	return S60_GetCalendar(s, Entry);
}

GSM_Error S60_DeleteCalendar(GSM_StateMachine *s, GSM_CalendarEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	sprintf(buffer, "%d", Entry->Location);

	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CALENDAR_ENTRY_DELETE, S60_TIMEOUT, ID_None);

	return error;
}

static GSM_Error S60_Reply_GetToDo(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;
	GSM_Error error;
	char *pos, *type, *content, *location, *start, *end, *modified, *replication, *alarm_time, *priority, *repeat, *repeat_rule, *repeat_exceptions, *repeat_start, *repeat_end, *interval, *crossedout, *crossedout_time;
	GSM_ToDoEntry *Entry;
	int i;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}

	/* Check for required fields */
	for (i = 0; i < 18; i++) {
		if (Priv->MessageParts[i] == NULL) {
			smprintf(s, "Not enough parts in reply!\n");
			return ERR_UNKNOWN;
		}
	}

	Entry = s->Phone.Data.ToDo;

	/* Grab values */
	pos = Priv->MessageParts[0];
	type = Priv->MessageParts[1];
	content = Priv->MessageParts[2];
	location = Priv->MessageParts[3];
	start = Priv->MessageParts[4];
	end = Priv->MessageParts[5];
	modified = Priv->MessageParts[6];
	replication = Priv->MessageParts[7];
	alarm_time = Priv->MessageParts[8];
	priority = Priv->MessageParts[9];
	repeat = Priv->MessageParts[10];
	repeat_rule = Priv->MessageParts[11];
	repeat_exceptions = Priv->MessageParts[12];
	repeat_start = Priv->MessageParts[13];
	repeat_end = Priv->MessageParts[14];
	interval = Priv->MessageParts[15];
	crossedout = Priv->MessageParts[16];
	crossedout_time = Priv->MessageParts[17];

	/* Check for correct type */
	if (strcmp(type, "todo") == 0) {
		Entry->Type = GSM_CAL_MEMO;
	} else {
		return ERR_EMPTY;
	}

	if (strlen(content) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_TEXT;
		DecodeUTF8(Entry->Entries[Entry->EntriesNum].Text, content, strlen(content));
		Entry->EntriesNum++;
	}

	if (strlen(location) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_LOCATION;
		DecodeUTF8(Entry->Entries[Entry->EntriesNum].Text, location, strlen(location));
		Entry->EntriesNum++;
	}

	if (strlen(start) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_START_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), start);
		Entry->EntriesNum++;
	}

	if (strlen(end) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_END_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), end);
		Entry->EntriesNum++;
	}

	if (strlen(modified) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_LAST_MODIFIED;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), modified);
		Entry->EntriesNum++;
	}

	if (strlen(replication) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_PRIVATE;
		if (strcmp(replication, "open") == 0) {
			Entry->Entries[Entry->EntriesNum].Number = 0;
		} else {
			Entry->Entries[Entry->EntriesNum].Number = 1;
		}
		Entry->EntriesNum++;
	}

	if (strlen(alarm_time) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_ALARM_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), alarm_time);
		Entry->EntriesNum++;
	}

	if (strlen(priority) > 0) {
		Entry->Priority = atoi(priority);
	}

	if (strlen(crossedout) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_COMPLETED;
		Entry->Entries[Entry->EntriesNum].Number = atoi(crossedout);
		Entry->EntriesNum++;
	}

	if (strlen(crossedout_time) > 0) {
		Entry->Entries[Entry->EntriesNum].EntryType = TODO_COMPLETED_DATETIME;
		GSM_DateTimeFromTimestamp(&(Entry->Entries[Entry->EntriesNum].Date), crossedout_time);
		Entry->EntriesNum++;
	}
	/* TODO: implement rest (repeating) */

	return ERR_NONE;
}

GSM_Error S60_GetToDo(GSM_StateMachine *s, GSM_ToDoEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	Entry->EntriesNum = 0;

	sprintf(buffer, "%d", Entry->Location);

	s->Phone.Data.ToDo = Entry;
	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CALENDAR_REQUEST_ENTRY, S60_TIMEOUT, ID_GetToDo);
	s->Phone.Data.ToDo = NULL;

	return error;
}

GSM_Error S60_GetNextToDo(GSM_StateMachine *s, GSM_ToDoEntry *Entry, gboolean Start)
{
	GSM_Error error;
	GSM_ToDoStatus Status;
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	if (Start) {
		error = S60_GetToDoStatus(s, &Status);
		if (error != ERR_NONE) {
			return error;
		}
		Priv->ToDoLocationsPos = 0;
	}

	if (Priv->ToDoLocations[Priv->ToDoLocationsPos] == 0) {
		return ERR_EMPTY;
	}

	Entry->Location = Priv->ToDoLocations[Priv->ToDoLocationsPos++];

	return S60_GetToDo(s, Entry);
}

GSM_Error S60_DeleteToDo(GSM_StateMachine *s, GSM_ToDoEntry *Entry)
{
	char buffer[100];
	GSM_Error error;

	sprintf(buffer, "%d", Entry->Location);

	error = GSM_WaitFor(s, buffer, strlen(buffer), NUM_CALENDAR_ENTRY_DELETE, S60_TIMEOUT, ID_None);

	return error;
}

GSM_Error S60_GetScreenshot(GSM_StateMachine *s, GSM_BinaryPicture *picture)
{
	GSM_Error error;

	s->Phone.Data.Picture = picture;
	error = GSM_WaitFor(s, NULL, 0, NUM_SCREENSHOT, S60_TIMEOUT, ID_Screenshot);
	s->Phone.Data.Picture = NULL;

	return error;
}

GSM_Error S60_Reply_Screenshot(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	s->Phone.Data.Picture->Type = PICTURE_PNG;
	s->Phone.Data.Picture->Buffer = (unsigned char *)malloc(msg.Length);
	if (s->Phone.Data.Picture->Buffer == NULL) {
		return ERR_MOREMEMORY;
	}
	s->Phone.Data.Picture->Length = DecodeBASE64(msg.Buffer, s->Phone.Data.Picture->Buffer, msg.Length);
	return ERR_NONE;
}

GSM_Error S60_GetSMSStatus(GSM_StateMachine *s, GSM_SMSMemoryStatus *status)
{
	GSM_Error error;

	status->SIMUnRead = 0;
	status->SIMUsed = 0;
	status->SIMSize = 0;
	status->TemplatesUsed = 0;
	status->PhoneUnRead = 0;
	status->PhoneUsed = 0;
	status->PhoneSize = 0;

	s->Phone.Data.SMSStatus = status;
	error = GSM_WaitFor(s, NULL, 0, NUM_MESSAGE_REQUEST_COUNT, S60_TIMEOUT, ID_GetSMSStatus);
	s->Phone.Data.SMSStatus = NULL;

	return error;
}

GSM_Error S60_Reply_GetSMSStatus(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	GSM_Error error;
	GSM_Phone_S60Data *Priv = &s->Phone.Data.Priv.S60;

	error = S60_SplitValues(&msg, s);
	if (error != ERR_NONE) {
		return error;
	}

	if (Priv->MessageParts[0] == NULL || Priv->MessageParts[1] == NULL) {
		return ERR_UNKNOWN;
	}
	s->Phone.Data.SMSStatus->PhoneUsed = atoi(Priv->MessageParts[0]);
	s->Phone.Data.SMSStatus->PhoneUnRead = atoi(Priv->MessageParts[1]);
	s->Phone.Data.SMSStatus->PhoneSize = s->Phone.Data.SMSStatus->PhoneUsed + 1000;

	return ERR_NONE;
}

GSM_Reply_Function S60ReplyFunctions[] = {

	{S60_Reply_Connect,	"", 0x00, NUM_CONNECTED, ID_Initialise },
	{S60_Reply_Generic,	"", 0x00, NUM_HELLO_REPLY, ID_EnableEcho },

	{S60_Reply_Generic,	"", 0x00, NUM_SYSINFO_REPLY_START, ID_GetModel },
	{S60_Reply_GetInfo,	"", 0x00, NUM_SYSINFO_REPLY_LINE, ID_GetModel },
	{S60_Reply_Generic,	"", 0x00, NUM_SYSINFO_REPLY_END, ID_GetModel },

	{S60_Reply_Generic,	"", 0x00, NUM_CONTACTS_REPLY_HASH_SINGLE_START, ID_GetMemoryStatus },
	{S60_Reply_ContactHash, "", 0x00, NUM_CONTACTS_REPLY_HASH_SINGLE_LINE, ID_GetMemoryStatus },
	{S60_Reply_Generic,	"", 0x00, NUM_CONTACTS_REPLY_HASH_SINGLE_END, ID_GetMemoryStatus },

	{S60_Reply_GetMemoryStatus,	"", 0x00, NUM_CONTACTS_REPLY_COUNT, ID_GetMemoryStatus },

	{S60_Reply_Generic,	"", 0x00, NUM_CALENDAR_REPLY_ENTRIES_START, ID_GetCalendarNotesInfo },
	{S60_Reply_CalendarCount, "", 0x00, NUM_CALENDAR_REPLY_ENTRY, ID_GetCalendarNotesInfo },
	{S60_Reply_Generic,	"", 0x00, NUM_CALENDAR_REPLY_ENTRIES_END, ID_GetCalendarNotesInfo },

	{S60_Reply_Generic,	"", 0x00, NUM_CALENDAR_REPLY_ENTRIES_START, ID_GetToDoInfo },
	{S60_Reply_ToDoCount, "", 0x00, NUM_CALENDAR_REPLY_ENTRY, ID_GetToDoInfo },
	{S60_Reply_Generic,	"", 0x00, NUM_CALENDAR_REPLY_ENTRIES_END, ID_GetToDoInfo },

	{S60_Reply_Generic, "", 0x00, NUM_CONTACTS_REPLY_CONTACT_START, ID_GetMemory },
	{S60_Reply_GetMemory, "", 0x00, NUM_CONTACTS_REPLY_CONTACT_LINE, ID_GetMemory },
	{S60_Reply_Generic, "", 0x00, NUM_CONTACTS_REPLY_CONTACT_END, ID_GetMemory },
	{S60_Reply_Generic, "", 0x00, NUM_CONTACTS_REPLY_CONTACT_NOT_FOUND, ID_GetMemory },

	{S60_Reply_GetCalendar, "", 0x00, NUM_CALENDAR_REPLY_ENTRY, ID_GetCalendarNote },
	{S60_Reply_Generic, "", 0x00, NUM_CALENDAR_REPLY_ENTRY_NOT_FOUND, ID_GetCalendarNote },

	{S60_Reply_GetToDo, "", 0x00, NUM_CALENDAR_REPLY_ENTRY, ID_GetToDo },
	{S60_Reply_Generic, "", 0x00, NUM_CALENDAR_REPLY_ENTRY_NOT_FOUND, ID_GetToDo },

	{S60_Reply_AddMemory, "", 0x00, NUM_CONTACTS_ADD_REPLY_ID, ID_SetMemory },

	{S60_Reply_GetNetworkInfo, "", 0x00, NUM_LOCATION_REPLY, ID_GetNetworkInfo },
	{S60_Reply_Generic, "", 0x00, NUM_LOCATION_REPLY_NA, ID_GetNetworkInfo },

	{S60_Reply_Generic, "", 0x00, NUM_QUIT, ID_Terminate },

	{S60_Reply_Screenshot, "", 0x00, NUM_SCREENSHOT_REPLY, ID_Screenshot },

	{S60_Reply_GetSMSStatus, "", 0x00,NUM_MESSAGE_REPLY_COUNT, ID_GetSMSStatus },

	{NULL,			"", 0x00, 0x00, ID_None }
};

GSM_Phone_Functions S60Phone = {
	"s60",
	S60ReplyFunctions,
	S60_Initialise,
	S60_Terminate,
	GSM_DispatchMessage,
	NOTIMPLEMENTED,			/* 	ShowStartInfo		*/
	S60_GetInfo,                 /*      GetManufacturer */
	S60_GetInfo,                 /*      GetModel */
	S60_GetInfo,                 /*      GetFirmware */
	S60_GetInfo,                 /*      GetIMEI */
	NOTIMPLEMENTED,			/*	GetOriginalIMEI		*/
	NOTIMPLEMENTED,			/*	GetManufactureMonth	*/
	NOTIMPLEMENTED,			/*	GetProductCode		*/
	NOTIMPLEMENTED,			/*	GetHardware		*/
	NOTIMPLEMENTED,			/*	GetPPM			*/
	NOTIMPLEMENTED,			/*	GetSIMIMSI		*/
	NOTIMPLEMENTED,			/*	GetDateTime		*/
	NOTIMPLEMENTED,			/*	SetDateTime		*/
	NOTIMPLEMENTED,			/*	GetAlarm		*/
	NOTIMPLEMENTED,			/*	SetAlarm		*/
	NOTSUPPORTED,			/* 	GetLocale		*/
	NOTSUPPORTED,			/* 	SetLocale		*/
	NOTIMPLEMENTED,			/*	PressKey		*/
	NOTIMPLEMENTED,			/*	Reset			*/
	NOTIMPLEMENTED,			/*	ResetPhoneSettings	*/
	NOTIMPLEMENTED,			/*	EnterSecurityCode	*/
	NOTIMPLEMENTED,			/*	GetSecurityStatus	*/
	NOTIMPLEMENTED,			/*	GetDisplayStatus	*/
	NOTIMPLEMENTED,			/*	SetAutoNetworkLogin	*/
	S60_GetBatteryCharge,
	S60_GetSignalQuality,
	S60_GetNetworkInfo,
	NOTIMPLEMENTED,     		/*  	GetCategory 		*/
 	NOTSUPPORTED,       		/*  	AddCategory 		*/
        NOTIMPLEMENTED,      		/*  	GetCategoryStatus 	*/
	S60_GetMemoryStatus,
	S60_GetMemory,
	S60_GetNextMemory,
	S60_SetMemory,
	S60_AddMemory,
	S60_DeleteMemory,
	NOTIMPLEMENTED,                 /*      DeleteAllMemory */
	NOTIMPLEMENTED,			/*	GetSpeedDial		*/
	NOTIMPLEMENTED,			/*	SetSpeedDial		*/
	NOTIMPLEMENTED,			/*	GetSMSC			*/
	NOTIMPLEMENTED,			/*	SetSMSC			*/
	S60_GetSMSStatus,
	NOTIMPLEMENTED,			/*	GetSMS			*/
	NOTIMPLEMENTED,			/*	GetNextSMS		*/
	NOTIMPLEMENTED,			/*	SetSMS			*/
	NOTIMPLEMENTED,			/*	AddSMS			*/
	NOTIMPLEMENTED,			/* 	DeleteSMS 		*/
	NOTIMPLEMENTED,			/*	SendSMSMessage		*/
	NOTSUPPORTED,			/*	SendSavedSMS		*/
	NOTSUPPORTED,			/*	SetFastSMSSending	*/
	NOTIMPLEMENTED,			/*	SetIncomingSMS		*/
	NOTIMPLEMENTED,			/* 	SetIncomingCB		*/
	NOTIMPLEMENTED,			/*	GetSMSFolders		*/
 	NOTIMPLEMENTED,			/* 	AddSMSFolder		*/
 	NOTIMPLEMENTED,			/* 	DeleteSMSFolder		*/
	NOTIMPLEMENTED,			/*	DialVoice		*/
        NOTIMPLEMENTED,			/*	DialService		*/
	NOTIMPLEMENTED,			/*	AnswerCall		*/
	NOTIMPLEMENTED,			/*	CancelCall		*/
 	NOTIMPLEMENTED,			/* 	HoldCall 		*/
 	NOTIMPLEMENTED,			/* 	UnholdCall 		*/
 	NOTIMPLEMENTED,			/* 	ConferenceCall 		*/
 	NOTIMPLEMENTED,			/* 	SplitCall		*/
 	NOTIMPLEMENTED,			/* 	TransferCall		*/
 	NOTIMPLEMENTED,			/* 	SwitchCall		*/
 	NOTIMPLEMENTED,			/* 	GetCallDivert		*/
 	NOTIMPLEMENTED,			/* 	SetCallDivert		*/
 	NOTIMPLEMENTED,			/* 	CancelAllDiverts	*/
	NOTIMPLEMENTED,			/*	SetIncomingCall		*/
	NOTIMPLEMENTED,			/*  	SetIncomingUSSD		*/
	NOTIMPLEMENTED,			/*	SendDTMF		*/
	NOTIMPLEMENTED,			/*	GetRingtone		*/
	NOTIMPLEMENTED,			/*	SetRingtone		*/
	NOTIMPLEMENTED,			/*	GetRingtonesInfo	*/
	NOTIMPLEMENTED,			/* 	DeleteUserRingtones	*/
	NOTIMPLEMENTED,			/*	PlayTone		*/
	NOTIMPLEMENTED,			/*	GetWAPBookmark		*/
	NOTIMPLEMENTED,			/* 	SetWAPBookmark 		*/
	NOTIMPLEMENTED, 		/* 	DeleteWAPBookmark 	*/
	NOTIMPLEMENTED,			/* 	GetWAPSettings 		*/
	NOTIMPLEMENTED,			/* 	SetWAPSettings 		*/
	NOTSUPPORTED,			/*	GetSyncMLSettings	*/
	NOTSUPPORTED,			/*	SetSyncMLSettings	*/
	NOTSUPPORTED,			/*	GetChatSettings		*/
	NOTSUPPORTED,			/*	SetChatSettings		*/
	NOTSUPPORTED,			/* 	GetMMSSettings		*/
	NOTSUPPORTED,			/* 	SetMMSSettings		*/
	NOTSUPPORTED,			/*	GetMMSFolders		*/
	NOTSUPPORTED,			/*	GetNextMMSFileInfo	*/
	NOTIMPLEMENTED,			/*	GetBitmap		*/
	NOTIMPLEMENTED,			/*	SetBitmap		*/
	S60_GetToDoStatus,
	S60_GetToDo,
	S60_GetNextToDo,
	NOTIMPLEMENTED,                 /*      SetTodo */
	NOTIMPLEMENTED,                 /*      AddTodo */
	NOTIMPLEMENTED,                 /*      DeleteTodo */
	NOTIMPLEMENTED,                 /*      DeleteAllTodo */
	S60_GetCalendarStatus,
	S60_GetCalendar,
	S60_GetNextCalendar,
	NOTIMPLEMENTED,                 /*      SetCalendar */
	NOTIMPLEMENTED,                 /*      AddCalendar */
	NOTIMPLEMENTED,                 /*      DeleteCalendar */
	NOTIMPLEMENTED,                 /*      DeleteAllCalendar */
	NOTSUPPORTED,			/* 	GetCalendarSettings	*/
	NOTSUPPORTED,			/* 	SetCalendarSettings	*/
	NOTIMPLEMENTED,                 /*      GetNoteStatus */
	NOTIMPLEMENTED,                 /*      GetNote */
	NOTIMPLEMENTED,                 /*      GetNextNote */
	NOTIMPLEMENTED,                 /*      SetNote */
	NOTIMPLEMENTED,                 /*      AddNote */
	NOTIMPLEMENTED,                 /*      DeleteNote */
	NOTIMPLEMENTED,                 /*      DeleteAllNotes */
	NOTIMPLEMENTED, 		/*	GetProfile		*/
	NOTIMPLEMENTED, 		/*	SetProfile		*/
    	NOTIMPLEMENTED,			/*  	GetFMStation        	*/
    	NOTIMPLEMENTED,			/*  	SetFMStation        	*/
    	NOTIMPLEMENTED,			/*  	ClearFMStations       	*/
	NOTIMPLEMENTED,                 /*      GetNextFileFolder */
	NOTIMPLEMENTED,			/*	GetFolderListing	*/
	NOTSUPPORTED,			/*	GetNextRootFolder	*/
	NOTSUPPORTED,			/*	SetFileAttributes	*/
	NOTIMPLEMENTED,                 /*      GetFilePart */
	NOTIMPLEMENTED,                 /*      AddFilePart */
	NOTIMPLEMENTED,                 /*      SendFilePart */
	NOTIMPLEMENTED, 		/* 	GetFileSystemStatus	*/
	NOTIMPLEMENTED,                 /*      DeleteFile */
	NOTIMPLEMENTED,                 /*      AddFolder */
	NOTIMPLEMENTED,                 /*      DeleteFile */		/* 	DeleteFolder		*/
	NOTSUPPORTED,			/* 	GetGPRSAccessPoint	*/
	NOTSUPPORTED,			/* 	SetGPRSAccessPoint	*/
	S60_GetScreenshot
};
#endif


/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */
