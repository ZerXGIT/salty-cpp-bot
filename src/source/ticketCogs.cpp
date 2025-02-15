//
// Created by julian on 11/4/22.
// When you pour water on a rock, nothing happens.

#include <regex>
#include <iostream>
#include <dpp/dpp.h>
#include <fmt/format.h>

#include "../include/utils.hpp"
#include "../include/ticketCogs.hpp"

using namespace std;

void ticket::init_ticket_commands(dpp::cluster &bot) {
    bot.log(dpp::ll_debug, "Initializing 'ticket_commands'");

    dpp::slashcommand ticket("ticket", "ticket base command", bot.me.id);

    ticket.set_default_permissions(dpp::permission(dpp::p_administrator));

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "create", "Create Ticket message at current/given channel.").
                    add_option(dpp::command_option(dpp::co_channel, "channel", "Define channel", false))
    );

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "set", "Set category for new tickets.")
                    .add_option(dpp::command_option(dpp::co_channel, "category", "Define category", false))
                    .add_option(dpp::command_option(dpp::co_role, "role", "Define Supporter/Moderator role", false))
                    .add_option(dpp::command_option(dpp::co_role, "role2", "Define Supporter/Moderator role2", false))
                    .add_option(dpp::command_option(dpp::co_role, "role3", "Define Supporter/Moderator role3", false))
                    .add_option(dpp::command_option(dpp::co_role, "role4", "Define Supporter/Moderator role4", false))
                    .add_option(dpp::command_option(dpp::co_role, "role5", "Define Supporter/Moderator role5", false))
    );

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "init", "Start configuration wizard.")
    );

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "config", "Open config embed")
    );

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "enable", "Enable the ticket system")
    );

    ticket.add_option(
            dpp::command_option(dpp::co_sub_command, "disable", "Disable the ticket system")
    );

    bot.global_command_create(ticket);
}


dpp::message ticket::create_ticket_message(size_t &channel_id, dpp::embed &embed) {
    return dpp::message(channel_id, embed)
            .add_component(dpp::component().add_component(dpp::component()
                                                                  .set_label("Create!")
                                                                  .set_type(dpp::cot_button)
                                                                  .set_emoji("📝")
                                                                  .set_id("ticket_create")));
}

void ticket::init_ticket_events(dpp::cluster &bot, mysqlpp::Connection &c) {
    bot.log(dpp::ll_debug, "Initializing 'ticket_events'");

    bot.on_button_click([&bot, &c](const dpp::button_click_t &event) {
        if (event.custom_id == "ticket_create") {
            // start thinking and then decide what happens later, don't let user wait
            // it's like a traffic light, you wait 2 seconds and everyone is mad at you.
            event.thinking(true);

            dpp::embed em;
            dpp::channel channel;
            size_t category_id, notify_id;
            string channel_mention;
            dpp::guild guild = event.command.get_guild();
            dpp::user user = event.command.usr;
            int ticket_id;

            mysqlpp::Query query = c.query();
            query << fmt::format("SELECT ticket.category_id, ticket.count, ticket.notify_channel FROM ticket WHERE ticket.server_id = '{0}'; "
              "UPDATE salty_cpp_bot.ticket SET count=count + 1 WHERE ticket.server_id = '{0}'", event.command.guild_id);


            mysqlpp::StoreQueryResult res = query.store();

            category_id = res[0]["category_id"];
            ticket_id = res[0]["count"];
            notify_id = res[0]["notify_channel"];

            u::kill_query(query);


            channel.set_name(fmt::format("ticket-{0}", ticket_id + 1))
                    .set_type(dpp::channel_type::CHANNEL_TEXT)
                    .set_guild_id(event.command.guild_id)
                    .set_parent_id(category_id);

            channel.add_permission_overwrite(event.command.usr.id, dpp::overwrite_type::ot_member,
                                                dpp::permissions::p_send_messages |
                                                dpp::permissions::p_view_channel, 0);

            channel.add_permission_overwrite(guild.id, dpp::overwrite_type::ot_role, 0, dpp::permissions::p_view_channel);


            bot.channel_create(channel, [&, event, notify_id](const dpp::confirmation_callback_t &confm) {
                if (confm.is_error()) {
                    confm_error<dpp::button_click_t>(bot, event, confm);

                    return;
                }

                auto t = confm.get<dpp::channel>();

                mysqlpp::Query query = c.query();

                query << fmt::format(
                        "insert into salty_cpp_bot.cur_tickets (server_id, user_id, ticket_id) values ({0}, {1}, {2})",
                        guild.id, user.id, t.id);

                query.execute();

                event.edit_original_response(dpp::message(
                        fmt::format("Ticket got created {0}", t.get_mention())
                ).set_flags(dpp::m_ephemeral));


                em.set_title(t.name)
                  .set_color(0xbc3440)
                  .set_description("Welcome to your ticket, a Moderator will be there shortly.")
                  .set_timestamp(time(nullptr));

                dpp::message msg(t.id, em);
                msg.add_component(dpp::component().add_component(
                                        dpp::component()
                                                .set_type(dpp::cot_button)
                                                .set_id("ticket_close")
                                                .set_label("Close ticket")
                                                .set_emoji("🔒")
                                                .set_style(dpp::cos_danger)));

                bot.message_create(msg);

                // notify mods when id is provided
                if (notify_id > 0) {
                    dpp::embed e;
                    e.set_title(fmt::format("{0} got created by {1}", t.name, event.command.usr.username))
                     .set_description(fmt::format("Claimed by: {0}\nTicket: {1}", "None", t.get_mention()))
                     .set_timestamp(time(nullptr))
                     .set_color(0xbc3440);

                    dpp::message mod_msg(notify_id, e);

                    mod_msg.add_component(dpp::component().add_component(dpp::component()
                                                            .set_type(dpp::cot_button)
                                                            .set_label("Claim")
                                                            .set_id("ticket_claim")
                                                            .set_emoji("📑")
                                                            .set_style(dpp::cos_success)));

                    bot.message_create(mod_msg);

                }
            });

        }

        if (event.custom_id == "ticket_delete") {
            bot.message_create(dpp::message(event.command.channel_id, "Ticket gets deleted in 1 sec."));
            try {
                bot.channel_delete_sync(event.command.channel_id);
            } catch (dpp::exception e) {}
        }

        if (event.custom_id == "ticket_close") {

            dpp::embed em;

            em.set_title("Are you sure that you want to close the Ticket?")
              .set_color(0xbc3440);

            dpp::message you_sure_msg(dpp::message(event.command.channel_id, em)
                                        .add_component(dpp::component()
                                            .add_component(
                                                dpp::component()
                                                    .set_label("Close it")
                                                    .set_type(dpp::cot_button)
                                                    .set_style(dpp::cos_danger)
                                                    .set_id("ticket_sure"))

                                            .add_component(
                                                dpp::component()
                                                    .set_label("Nah, let it open")
                                                    .set_type(dpp::cot_button)
                                                    .set_style(dpp::cos_secondary)
                                                    .set_id("ticket_sure_nah"))));

            event.reply(you_sure_msg);
        }

        if (event.custom_id == "ticket_sure_nah") {
            bot.message_delete(event.command.message_id, event.command.channel_id);
        }

        if (event.custom_id == "ticket_sure") {
            dpp::embed em;
            dpp::channel channel = event.command.get_channel();

            event.reply(dpp::message("ticket gets closed"));

            mysqlpp::Query query = c.query();
            query << fmt::format("select user_id from cur_tickets where server_id = {0} and ticket_id = {1};",
                                 event.command.guild_id, event.command.channel_id);

            mysqlpp::StoreQueryResult res = query.store();

            long user_id = res[0]["user_id"];
            u::kill_query(query);

            bot.channel_edit_permissions(channel, user_id, 0, dpp::permissions::p_view_channel, true);

        }

        if (event.custom_id == "ticket_reopen") {}

        if (event.custom_id == "ticket_archive") {}

        if (event.custom_id == "ticket_claim") {

        }

    });
}

template<typename T>
void ticket::confm_error(const dpp::cluster &bot, const T &event,
                         const dpp::confirmation_callback_t &confm) {
    string err = confm.get_error().message;
    event.edit_original_response(
            dpp::message(
                    "Something went wrong, please try again later, or contact an Moderator.")
                    .set_flags(dpp::m_ephemeral));

    // not a typo
    bot.log(dpp::ll_error, fmt::format("FUCK Somethin went wron {}", err));
}

void ticket::ticket_commands(dpp::cluster &bot,
                             const dpp::slashcommand_t &event,
                             const dpp::command_interaction &cmd_data,
                             mysqlpp::Connection &c) {

    auto sc = cmd_data.options[0];

    dpp::embed em;
    dpp::guild guild = event.command.get_guild();
    std::string guild_icon = guild.get_icon_url();


    if (sc.name == "create") {
        mysqlpp::Query query = c.query();
        string title;

        {
            query << fmt::format("if not exists(select * from ticket where server_id='{0}') then"
                                    " insert into ticket (server_id) values ('{0}');"
                                 "end if; ", event.command.guild_id);

            query.execute();
        }

        query << fmt::format("select ticket.ticket_title from ticket where ticket.server_id = '{0}';",
                             event.command.guild_id);

        mysqlpp::StoreQueryResult res = query.store();

        title = (string) res[0]["ticket_title"];

        u::kill_query(query);

        em.set_color(0xbc3440)
          .set_title(title)
          .set_description("Create a ticket with 📝")
          .set_footer("Kenexar.eu", bot.me.get_avatar_url())
          .set_timestamp(time(nullptr));

        try {
            if (!sc.options.empty()) {
                std::size_t channel_id = sc.get_value<dpp::snowflake>(0);
                bot.message_create(ticket::create_ticket_message(channel_id, em));
            } else {
                std::size_t channel_id = event.command.channel_id;
                bot.message_create(ticket::create_ticket_message(channel_id, em));
            }
            event.reply(dpp::message("Ticket message got created.").set_flags(dpp::m_ephemeral));

        } catch (std::exception e) {
            bot.log(dpp::ll_error, fmt::format("Oh no, not good {}", e.what()));

            event.reply("Something went wrong, try again later.");
        }

    }

    if (sc.name == "set" and !sc.options.empty()) {
        short count = 0;
        stringstream ss;

        for (auto& i : sc.options) {
            if (i.name.find("role") != string::npos) {
                ss << "Roles ";

                std::size_t role_id = sc.get_value<dpp::snowflake>(count);
                mysqlpp::Query query = c.query();

                ss << fmt::format("<@&{0}> ", role_id);

                query << fmt::format(
                    "if not exists(select * from ticket_access_roles where role_id='{1}') then"
                        " insert into ticket_access_roles (server_id, role_id) values ('{0}', '{1}');"
                    "end if; ", event.command.guild_id, role_id);

                query.execute();

                ss << "for support added. ";
            }

            if (i.name == "category") {
                size_t category_id = sc.get_value<dpp::snowflake>(count);

                bot.channels_get(event.command.guild_id, [&, event](const dpp::confirmation_callback_t &confm) {
                    if (confm.is_error()) {
                        confm_error<dpp::slashcommand_t>(bot, event, confm);
                        return;
                    }

                    mysqlpp::Query query = c.query();

                    query << fmt::format("if exists(select * from salty_cpp_bot.ticket where server_id = {0}) then"
                                " update salty_cpp_bot.ticket set category_id = {1} where server_id = {0}; "
                                "else"
                                " insert into salty_cpp_bot.ticket (server_id, category_id) values ({0}, {1}); "
                                "end if;",
                            event.command.guild_id, category_id);

                    query.execute();
                });
                ss << fmt::format("Ticket category changed to: '<#{0}>'. ", category_id);
            }
            count++;
        }


        event.reply(dpp::message(ss.str()).set_flags(dpp::m_ephemeral));

    }
}