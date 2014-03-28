/**
 * @file avstore.cpp Implementation of store for Authentication Vectors
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include <map>
#include <pthread.h>

#include "log.h"
#include "store.h"
#include "avstore.h"
#include "sas.h"
#include "sasevent.h"

AvStore::AvStore(Store* data_store) :
  _data_store(data_store)
{
}


AvStore::~AvStore()
{
}


bool AvStore::set_av(const std::string& impi,
                     const std::string& nonce,
                     const Json::Value* av,
                     SAS::TrailId trail)
{
  std::string key = impi + '\\' + nonce;
  Json::FastWriter writer;
  std::string data = writer.write(*av);
  LOG_DEBUG("Set AV for %s\n%s", key.c_str(), data.c_str());
  Store::Status status = _data_store->set_data("av", key, data, 0, AV_EXPIRY, trail);

  if (status != Store::Status::OK)
  {
    // LCOV_EXCL_START
    LOG_ERROR("Failed to write Authentication Vector for private_id %s", impi.c_str());

    SAS::Event event(trail, SASEvent::AVSTORE_FAILURE, 0);
    event.add_var_param("SET");
    event.add_var_param(key);
    event.add_static_param(status);
    SAS::report_event(event);

    return false;
    // LCOV_EXCL_STOP
  }

  SAS::Event event(trail, SASEvent::AVSTORE_SUCCESS, 0);
  event.add_var_param("SET");
  event.add_var_param(key);
  event.add_var_param(data);
  SAS::report_event(event);

  return true;
}

bool AvStore::delete_av(const std::string& impi,
                        const std::string& nonce,
                        SAS::TrailId trail)
{
  std::string key = impi + '\\' + nonce;
  LOG_DEBUG("Delete AV for %s", key.c_str());
  Store::Status status = _data_store->delete_data("av", key, trail);

  if (status != Store::Status::OK)
  {
    // LCOV_EXCL_START
    LOG_ERROR("Failed to delete Authentication Vector for private_id %s", impi.c_str());

    SAS::Event event(trail, SASEvent::AVSTORE_FAILURE, 0);
    event.add_var_param("DELETE");
    event.add_var_param(key);
    event.add_static_param(status);
    SAS::report_event(event);

    return false;
    // LCOV_EXCL_STOP
  }

  SAS::Event event(trail, SASEvent::AVSTORE_SUCCESS, 0);
  event.add_var_param("DELETE");
  event.add_var_param(key);
  event.add_var_param("");
  SAS::report_event(event);

  return true;
}


Json::Value* AvStore::get_av(const std::string& impi,
                             const std::string& nonce,
                             SAS::TrailId trail)
{
  Json::Value* av = NULL;
  std::string key = impi + '\\' + nonce;
  std::string data;
  uint64_t cas;
  Store::Status status = _data_store->get_data("av", key, data, cas, trail);

  if (status == Store::Status::OK)
  {
    LOG_DEBUG("Retrieved AV for %s\n%s", key.c_str(), data.c_str());
    av = new Json::Value;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(data, *av);
    if (!parsingSuccessful)
    {
      LOG_DEBUG("Failed to parse AV\n%s",
                reader.getFormattedErrorMessages().c_str());
      delete av;
      av = NULL;
    }

    SAS::Event event(trail, SASEvent::AVSTORE_SUCCESS, 0);
    event.add_var_param("GET");
    event.add_var_param(key);
    event.add_var_param(data);
    SAS::report_event(event);
  }
  else
  {
    SAS::Event event(trail, SASEvent::AVSTORE_FAILURE, 0);
    event.add_var_param("GET");
    event.add_var_param(key);
    event.add_static_param(status);
    SAS::report_event(event);
  }

  return av;
}

