//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2018         OpenVPN, Inc. <sales@openvpn.net>
//  Copyright (C) 2018         David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as
//  published by the Free Software Foundation, version 3 of the
//  License.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 * @file   config.hpp
 *
 * @brief  Implementation of all the various openvpn3 config-* commands
 */

#include "../cmdargparser.hpp"
#include "../lookup.hpp"


/**
 *  Parses and imports an OpenVPN configuration file and saves it
 *  within the OpenVPN 3 Configuration Manager service.
 *
 *  This parser will also ensure all external files are embedded into
 *  the configuration before sent to the configuration manager.
 *
 *  @param filename    Filename of the configuration file to import
 *  @param cfgname     Configuration name to be used inside the configuration manager
 *  @param single_use  This will make the Configuration Manager automatically
 *                     delete the configuration file from its storage upon the first
 *                     use.  This is used for load-and-connect scenarios where it
 *                     is not likely the configuration will be re-used
 *  @param persistent  This will make the Configuration Manager store the configuration
 *                     to disk, to be re-used later on.
 *
 *  @return Retuns a string containing the D-Bus object path to the configuration
 */
std::string import_config(const std::string filename,
                          const std::string cfgname,
                          const bool single_use,
                          const bool persistent)
{
    // Parse the OpenVPN configuration
    // The ProfileMerge will ensure that all needed
    // files are embedded into the configuration we
    // send to and store in the Configuration Manager
    ProfileMerge pm(filename, "", "",
                    ProfileMerge::FOLLOW_FULL,
                    ProfileParseLimits::MAX_LINE_SIZE,
                    ProfileParseLimits::MAX_PROFILE_SIZE);

    // Import the configuration file
    OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration);
    return conf.Import(cfgname, pm.profile_content(), single_use, persistent);
}


/**
 *  openvpn3 config-import command
 *
 *  Imports a configuration file into the Configuration Manager.  This
 *  import operation will also embedd all external files into the imported
 *  profile as well.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_import(ParsedArgs args)
{
    if (!args.Present("config"))
    {
        throw CommandException("config-import", "Missing required --config option");
    }
    try
    {
        std::string name = (args.Present("name") ? args.GetValue("name", 0)
                                                : args.GetValue("config", 0));
        std::string path = import_config(args.GetValue("config", 0),
                                         name,
                                         false,
                                         args.Present("persistent"));
        std::cout << "Configuration imported.  Configuration path: "
                  << path
                  << std::endl;
        return 0;
    }
    catch (std::exception e)
    {
        throw CommandException("config-import", e.what());
    }
}


/**
 *  openvpn3 config-list command
 *
 *  Lists all available configuration profiles.  Only profiles where the
 *  calling user is the owner, have been added to the access control list
 *  or profiles tagged with public_access will be listed.  This restriction
 *  is handled by the Configuration Manager.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_list(ParsedArgs args)
{
    OpenVPN3ConfigurationProxy confmgr(G_BUS_TYPE_SYSTEM, OpenVPN3DBus_rootp_configuration );

    std::cout << "Configuration path" << std::endl;
    std::cout << "Name" << std::setw(32-4) << " "
              << "Alias" << std::setw(16-5) << " "
              << "Owner" << std::setw(16-5)
              << std::endl;
    std::cout << std::setw(32+16+16+2) << std::setfill('-') << "-" << std::endl;

    bool first = true;
    for (auto& cfg : confmgr.FetchAvailableConfigs())
    {
        if (cfg.empty())
        {
            continue;
        }
        OpenVPN3ConfigurationProxy cprx(G_BUS_TYPE_SYSTEM, cfg);

        if (!first)
        {
            std::cout << std::endl;
        }
        first = false;

        std::string name = cprx.GetStringProperty("name");
        std::string alias = cprx.GetStringProperty("alias");
        std::string user = lookup_username(cprx.GetUIntProperty("owner"));

        std::cout << cfg << std::endl;
        std::cout << name << std::setw(32 - name.size()) << std::setfill(' ') << " "
                  << alias << std::setw(16 - alias.size()) << " "
                  << user << std::setw(16)
                  << std::endl;
    }
    std::cout << std::setw(32+16+16+3-1) << std::setfill('-') << "-" << std::endl;
    return 0;
}


/**
 *  openvpn3 config-alias command
 *
 *  Manages configuration profile aliases.  An alias can be used instead of
 *  the full D-Bus object path.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_alias(ParsedArgs args)
{
    if (!args.Present("path"))
    {
        throw CommandException("config-alias", "No configuration path provided");
    }

    if (!args.Present("name") && !args.Present("delete"))
    {
        throw CommandException("config-alias", "Alias --name or --delete argument provided");
    }

    if (args.Present("name") && args.Present("delete"))
    {
        throw CommandException("Cannot provide both --name and --delete at the same time");
    }

    try
    {
        std::string path = args.GetValue("path", 0);
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM, path);

        if (args.Present("name"))
        {
            std::string alias = args.GetValue("name", 0);
            conf.SetAlias(alias);
            std::cout << "Alias set to '" << alias << "' " << std::endl;
        }
        else if (args.Present("delete"))
        {
            throw CommandException("config-alias",
                                   "Deleting configuration aliases is not yet implemented");
            conf.SetAlias("");
            std::cout << "Alias is deleted" << std::endl;
        }
        return 0;
    }
    catch (DBusPropertyException& err)
    {
        throw CommandException("config-alias", err.getRawError());
    }
    catch (DBusException& err)
    {
        throw CommandException("config-alias", err.getRawError());
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  openvpn3 config-acl command
 *
 *  Command to modify the access control to a specific configuration profile.
 *  All operations related to granting, revoking, public-access, locking-down
 *  and sealing (making configurations read-only) are handled by this command.
 *
 *  Also note that you can run multiple operarations in a single command line.
 *  It is fully possible to use --grant, --revoke and --show in a single
 *  command line.  In addition, both --grant and --revoke can be used multiple
 *  times to grant/revoke several users in a single operation.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_acl(ParsedArgs args)
{
    int ret = 0;
    if (!args.Present("path"))
    {
        throw CommandException("config-acl", "No configuration path provided");
    }

    if (!args.Present("show")
        && !args.Present("grant")
        && !args.Present("revoke")
        && !args.Present("public-access")
        && !args.Present("lock-down")
        && !args.Present("seal"))
    {
        throw CommandException("config-acl", "No operation option provided");
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM,
                                        args.GetValue("path", 0));
        if (args.Present("grant"))
        {
            for (auto const& user : args.GetAllValues("grant"))
            {
                uid_t uid = get_userid(user);
                // If uid == -1, something is wrong
                if (-1 == uid)
                {
                    std::cerr << "** ERROR ** --grant " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
                else
                {
                    // We have a UID ... now grant the access
                    try
                    {
                        conf.AccessGrant(uid);
                        std::cout << "Granted access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException& e)
                    {
                        std::cerr << "Failed granting access to "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }
                }
            }
        }

        if (args.Present("revoke"))
        {
            for (auto const& user : args.GetAllValues("revoke"))
            {
                uid_t uid = get_userid(user);
                // If uid == -1, something is wrong
                if (-1 == uid)
                {
                    std::cerr << "** ERROR ** --grant " << user
                              << " does not map to a valid user account"
                              << std::endl;
                }
                else {
                    // We have a UID ... now grant the access
                    try
                    {
                        conf.AccessRevoke(uid);
                        std::cout << "Access revoked from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                    }
                    catch (DBusException& e)
                    {
                        std::cerr << "Failed revoking access from "
                                  << lookup_username(uid)
                                  << " (uid " << uid << ")"
                                  << std::endl;
                        ret = 3;
                    }

                }
            }
        }

        if (args.Present("lock-down"))
        {
            std::string ld = args.GetValue("lock-down", 0);
            if (("false" == ld) || ("true" == ld ))
            {
                conf.SetLockedDown("true" == ld);
                if ("true" == ld)
                {
                    std::cout << "Configuration has been locked down"
                              << std::endl;
                }
                else
                {
                    std::cout << "Configuration has been opened up"
                              << std::endl;

                }
            }
            else
            {
                std::cerr << "** ERROR ** --lock-down argument must be "
                          << "'true' or 'false'"
                          << std::endl;
                ret = 3;
            }
        }


        if (args.Present("public-access"))
        {
            std::string ld = args.GetValue("public-access", 0);
            if (("false" == ld) || ("true" == ld ))
            {
                conf.SetPublicAccess("true" == ld);
                if ("true" == ld)
                {
                    std::cout << "Configuration is now readable to everyone"
                              << std::endl;
                }
                else
                {
                    std::cout << "Configuration is now readable to only "
                              << "specific users"
                              << std::endl;

                }
            }
            else
            {
                std::cerr << "** ERROR ** --public-access argument must be "
                          << "'true' or 'false'"
                          << std::endl;
                ret = 3;
            }
        }

        if (args.Present("seal"))
        {
            std::cout << "This operation CANNOT be undone and makes this "
                      << "configuration profile read-only."
                      << std::endl;
            std::cout << "Are you sure you want to do this? "
                      << "(enter yes in upper case) ";

            std::string response;
            std::cin >> response;
            if ("YES" == response)
            {
                conf.Seal();
                std::cout << "Configuration has been sealed." << std::endl;
            }
            else
            {
                std::cout << "--seal operation has been cancelled"
                          << std::endl;
            }
        }

        if (args.Present("show"))
        {
            std::cout << "    Configuration name: "
                      << conf.GetStringProperty("name")
                      << std::endl;

            std::string owner = lookup_username(conf.GetOwner());
            std::cout << "                 Owner: ("
                      << conf.GetOwner() << ") "
                      << " " << ('(' != owner[0] ? owner : "(unknown)")
                      << std::endl;

            std::cout << "             Read-only: "
                      << (conf.GetBoolProperty("readonly") ? "yes" : "no")
                      << std::endl;

            std::cout << "         Public access: "
                      << (conf.GetPublicAccess() ? "yes" : "no")
                      << std::endl;

            if (!conf.GetPublicAccess())
            {
                std::vector<uid_t> acl = conf.GetAccessList();
                std::cout << "  Users granted access: " << std::to_string(acl.size())
                          << (1 != acl.size() ? " users" : " user")
                          << std::endl;
                for (auto const& uid : acl)
                {
                    std::string user = lookup_username(uid);
                    std::cout << "                        - (" << uid << ") "
                              << " " << ('(' != user[0] ? user : "(unknown)")
                    << std::endl;
                }
            }
        }
        return ret;
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  openvpn3 config-show command
 *
 *  This command is used to show the contents of a configuration profile.
 *  It allows both the textual representation, which is compatible with
 *  OpenVPN 2.x based clients as well as JSON by providing the --json option.
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_show(ParsedArgs args)
{
    if (!args.Present("path"))
    {
        throw CommandException("config-show", "No configuration path provided");
    }

    try
    {
        OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM,
                                        args.GetValue("path", 0));

        if (!args.Present("json"))
        {
            std::cout << "Configuration: " << std::endl
                      << "  - Name:       " << conf.GetStringProperty("name") << std::endl
                      << "  - Read only:  " << (conf.GetBoolProperty("readonly") ? "Yes" : "No") << std::endl
                      << "  - Persistent: " << (conf.GetBoolProperty("persistent") ? "Yes" : "No") << std::endl
                      << "--------------------------------------------------" << std::endl
                      << conf.GetConfig() << std::endl
                      << "--------------------------------------------------" << std::endl;
        }
        else
        {
            std::cout << conf.GetJSONConfig() << std::endl;
        }
        return 0;
    }
    catch (DBusException& err)
    {
        throw CommandException("config-show", err.getRawError());
    }
}


/**
 *  openvpn3 config-remove command
 *
 *  This command will delete and remove a configuration profile from the
 *  Configuration Manager
 *
 * @param args  ParsedArgs object containing all related options and arguments
 * @return Returns the exit code which will be returned to the calling shell
 *
 */
static int cmd_config_remove(ParsedArgs args)
{
    if (!args.Present("path"))
    {
        throw CommandException("config-remove", "No configuration path provided");
    }

    try
    {
        std::cout << "This operation CANNOT be undone and removes this"
                  << "configuration profile completely."
                  << std::endl;
        std::cout << "Are you sure you want to do this? "
                  << "(enter yes in upper case) ";

        std::string response;
        std::cin >> response;
        if ("YES" == response)
        {
            OpenVPN3ConfigurationProxy conf(G_BUS_TYPE_SYSTEM,
                                            args.GetValue("path", 0));
            conf.Remove();
            std::cout << "Configuration removed." << std::endl;
        }
        else
        {
            std::cout << "Configuration profile delete operating cancelled"
                      << std::endl;
        }
        return 0;
    }
    catch (DBusException& err)
    {
        throw CommandException("config-remove", err.getRawError());
    }
    catch (...)
    {
        throw;
    }
}


/**
 *  Declare all the supported commands and their options and arguments.
 *
 *  This function should only be called once by the main openvpn3 program,
 *  which sends a reference to the Commands argument parser which is used
 *  for this registration process
 *
 * @param ovpn3  Commands object where to register all the commands, options
 *               and arguments.
 */
void RegisterCommands_config(Commands& ovpn3)
{
    //
    //  config-import command
    //
    auto cmd = ovpn3.AddCommand("config-import",
                                "Import configuration profiles",
                                cmd_config_import);
    cmd->AddOption("config", 'c', "CFG-FILE", true,
                   "Configuration file to import");
    cmd->AddOption("name", 'n', "NAME", true,
                   "Provide a different name for the configuration (default: CFG-FILE)");
    cmd->AddOption("persistent", 'p',
                   "Make the configuration file persistent through boots");

    //
    //  config-list command
    //
    cmd = ovpn3.AddCommand("config-list",
                           "List all available configuration profiles",
                           cmd_config_list);

    cmd = ovpn3.AddCommand("config-alias",
                           "Manage configuration aliases",
                           cmd_config_alias);
    cmd->AddOption("path", 'o', "OBJ-PATH", true,
                   "Path to the configuration in the configuration manager");
    cmd->AddOption("name", 'n', "ALIAS-NAME", true,
                   "Alias name to use for this configuration");
    cmd->AddOption("delete", 'D',
                   "Delete this alias");

    //
    //  config-acl command
    //
    cmd = ovpn3.AddCommand("config-acl",
                           "Manage access control lists for configurations",
                           cmd_config_acl);
    cmd->AddOption("path", 'o', "OBJ-PATH", true,
                   "Path to the configuration in the configuration manager");
    cmd->AddOption("show", 's',
                   "Show the current access control lists");
    cmd->AddOption("grant", 'G', "<UID | username>", true,
                   "Grant this user access to this configuration profile");
    cmd->AddOption("revoke", 'R', "<UID | username>", true,
                   "Revoke this user access from this configuration profile");
    cmd->AddOption("public-access", "<true|false>", true,
                   "Set/unset the public access flag");
    cmd->AddOption("lock-down", "<true|false>", true,
                   "Set/unset the lock-down flag.  Will disable config retrieval for users");
    cmd->AddOption("seal", 'S',
                   "Make the configuration profile permanently read-only");
    //
    //  config-show command
    //
    cmd = ovpn3.AddCommand("config-show",
                           "Show/dump a configuration profile",
                           cmd_config_show);
    cmd->AddOption("path", 'o', "OBJ-PATH", true,
                   "Path to the configuration in the configuration manager");
    cmd->AddOption("json", 'j', "Dump the configuration in JSON format");

    //
    //  config-remove command
    //
    cmd = ovpn3.AddCommand("config-remove",
                           "Remove an available configuration profile",
                           cmd_config_remove);
    cmd->AddOption("path", 'o', "OBJ-PATH", true,
                   "Path to the configuration in the configuration manager");
}