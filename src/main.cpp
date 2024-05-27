#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <ncurses.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include "form.h"

// pre-declare functions to avoid warnings in the main function
struct Post;
std::vector<Post> fetch_and_parse_posts();
std::string fetch_author_name(int author_id);
void init_ncurses();
void display_post(const Post& post, int offset, WINDOW* content_win);
void handle_user_input(std::vector<Post>& posts, int& index, int& offset, WINDOW* sidebar_win, WINDOW* content_win);
void display_sidebar(const std::vector<Post>& posts, int current_index, int offset, WINDOW* sidebar_win);
void display_posts(const std::vector<Post>& posts);
char* trim_whitespaces(char* str);
bool login_and_save_token(const std::string& username, const std::string& password);
void post_request_with_token(const std::string& title, const std::string& content);
void create_post(const std::string& title);

const std::string URL = "http://127.0.0.1:8000";

WINDOW* popup_window = nullptr; // 悬浮窗口的引用

enum WindowState {
    BLOG_VIEW,
    LOGIN_VIEW,
};

WindowState current_state = BLOG_VIEW; // 初始状态设置为博客视图

struct Post {
    std::string title;
    std::string content;
    int id;
    std::string published;
    int author_id;
    std::string author_name;  // 新增字段
};


std::vector<Post> fetch_and_parse_posts() {
    // API 端点
    std::string url = "http://127.0.0.1:8000/posts";

    // 发送 GET 请求
    cpr::Response response = cpr::Get(cpr::Url{url});

    // 检查 HTTP 响应状态码
    if (response.status_code != 200) {
        std::cerr << "Failed to fetch posts: HTTP " << response.status_code << std::endl;
        return {};
    }

    // 解析 JSON 响应
    auto json_response = nlohmann::json::parse(response.text);
    std::vector<Post> posts;

    // 迭代 JSON 数组并构造 Post 结构体列表
    for (const auto& item : json_response) {
        Post post{
                item["title"].get<std::string>(),
                item["content"].get<std::string>(),
                item["id"].get<int>(),
                item["published"].get<std::string>(),
                item["author_id"].get<int>(),
                fetch_author_name(item["author_id"].get<int>())  // 获取作者名
        };
        posts.push_back(post);
    }

    return posts;
}

std::string fetch_author_name(int author_id) {
    std::string url = URL + "/users/" + std::to_string(author_id);
    auto response = cpr::Get(cpr::Url{url});

    if (response.status_code == 200) {
        auto json_response = nlohmann::json::parse(response.text);
        return json_response["username"].get<std::string>();
    } else {
        return "Unknown Author";  // 如果请求失败或找不到用户，返回未知作者
    }
}

bool login_and_save_token(const std::string& username, const std::string& password) {
    try {
        // 发送POST请求
        cpr::Response response = cpr::Post(
                cpr::Url{URL+"/login"},
                cpr::Payload{{"username", username}, {"password", password}}
                //cpr::Header{{"Content-Type", "application/x-www-form-urlencoded"}}
        );

        // 检查HTTP响应状态码
        if (response.status_code == 200) {
            // 解析JSON响应体
            auto resp_json = nlohmann::json::parse(response.text);

            // 检查JSON对象是否包含"access_token"
            if (resp_json.contains("access_token")) {
                std::string token = resp_json["access_token"].get<std::string>();

                // 将令牌保存到文件中
                std::ofstream token_file("token");
                if (token_file.is_open()) {
                    token_file << token;
                    token_file.close();
                    return true; // 成功保存令牌
                } else {
                    std::cerr << "Failed to open token file for writing.\n";
                    return false; // 文件打开失败
                }
            } else {
                std::cerr << "Login successful but no access token provided.\n";
                return false;
            }
        } else {
            // 输出错误消息
            std::cerr << "Failed to login, status code: " << response.status_code << "\n";
            std::cerr << "Response: " << response.text << "\n";
            return false; // 登录失败
        }
    } catch (const std::exception& e) {
        // 捕获并输出异常信息
        std::cerr << "Exception occurred: " << e.what() << "\n";
        return false; // 异常情况
    }
}

//define a function to make post request with token, the token is stored in a file named "token". the form of data is json, which contains two keys: "title" and "content"
void post_request_with_token(const std::string& title, const std::string& content) {
    std::ifstream token_file("token");
    if (!token_file.is_open()) {
        std::cerr << "Failed to open token file for reading.\n";
        return;
    }

    std::string token;
    std::getline(token_file, token);
    token_file.close();

    std::string url = URL + "/posts";

    nlohmann::json data = {
        {"title", title},
        {"content", content}
    };

    cpr::Response response = cpr::Post(
            cpr::Url{url},
            cpr::Header{{"Authorization", "Bearer " + token}},
            cpr::Body{data.dump()},
            cpr::Header{{"Content-Type", "application/json"}}
    );

    if (response.status_code == 201) {
        //std::cout << "Post created successfully.\n";
    } else {
        //std::cerr << "Failed to create post, status code: " << response.status_code << "\n";
        //std::cerr << "Response: " << response.text << "\n";
    }
}

//define a function to receive title as parameter and read content from post.txt file, then call post_request_with_token function to make post request
void create_post(const std::string& title) {
    // 确保标题有效性
    if (title.empty()) {
        //std::cerr << "Title cannot be empty.\n";
        return;
    }

    std::ifstream post_file("./post.txt");
    if (!post_file.is_open()) {
        std::cerr << "Failed to open post file for reading.\n";
        return;
    }

    std::string content((std::istreambuf_iterator<char>(post_file)), std::istreambuf_iterator<char>());

    // 检查内容是否为空
    if (content.empty()) {
        std::cerr << "Content is empty, nothing to post.\n";
        return;
    }

    post_request_with_token(title, content);
}


void init_ncurses() {
    initscr();          // 开始 ncurses 模式
    cbreak();           // 行缓冲禁用，传递所有控制信息
    noecho();           // 不显示输入的字符
    keypad(stdscr, TRUE); // 启用键盘映射
    scrollok(stdscr, TRUE); // 允许窗口滚动

    if (has_colors()) { // 检查终端是否支持颜色
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);    // 为侧边栏设置颜色对
        init_pair(2, COLOR_BLACK, COLOR_WHITE);  // 为内容区域设置颜色对
    }

    refresh();          // 刷新窗口以显示初始屏幕
}


void display_post(const Post& post, int offset, WINDOW* content_win) {
    werase(content_win);

    int max_y, max_x;
    getmaxyx(content_win, max_y, max_x);

    int line = 1;
    std::string title = post.title;
    int start_pos = (max_x - title.length()) / 2;
    mvwprintw(content_win, 0, start_pos > 0 ? start_pos : 0, "%s", title.c_str());

    std::istringstream content_stream(post.content);
    std::string content_line;
    while (std::getline(content_stream, content_line, '\n')) {
        size_t pos = 0;
        while ((pos = content_line.find('\t', pos)) != std::string::npos) {
            content_line.replace(pos, 1, "    ");
            pos += 4;
        }

        int start_index = 0;
        while (content_line.length() > start_index) {
            if (line >= max_y - 1) break;
            mvwprintw(content_win, line++, 0, "%s", content_line.substr(start_index, max_x).c_str());
            start_index += max_x;
        }
    }

    if (line < max_y) {
        mvwprintw(content_win, max_y - 1, 0, "Author: %s, Published: %s", post.author_name.c_str(), post.published.c_str());
    }

    wrefresh(content_win);
}


/*
void handle_view_change_input() {
    int ch = getch();
    switch (ch) {
        case KEY_F(1):
            if (current_state == BLOG_VIEW) {
                current_state = LOGIN_VIEW; // 切换到空窗口状态
                // 创建或显示悬浮窗口
                if (!popup_window) {
                    popup_window = newwin(10, 30, 5, 5); // 创建新窗口，尺寸和位置可以根据需要调整
                    box(popup_window, 0 , 0); // 给悬浮窗口加边框
                    mvwprintw(popup_window, 1, 1, "This is a popup window.");
                    mvwprintw(popup_window, 2, 1, "Press F1 to close.");
                }
                wrefresh(popup_window); // 刷新悬浮窗口，使其显示
            } else {
                current_state = BLOG_VIEW; // 返回博客视图
                // 隐藏悬浮窗口
                if (popup_window) {
                    werase(popup_window); // 清除悬浮窗口内容
                    wrefresh(popup_window); // 刷新窗口以显示清除后的状态
                    delwin(popup_window); // 删除窗口
                    popup_window = nullptr; // 重置窗口指针
                }
            }
            break;
    }
}
*/

void handle_user_input(std::vector<Post>& posts, int& index, int& offset,int& sidebar_offset, WINDOW* sidebar_win, WINDOW* content_win) {
    int ch = getch();
    switch (ch) {
        case KEY_F(1):
            if (current_state == BLOG_VIEW) {
                current_state = LOGIN_VIEW; // 切换到登录窗口状态

                // 创建或显示悬浮窗口
                if (!popup_window) {
                    popup_window = newwin(10, 30, 6, 10); // 创建新窗口，尺寸和位置可以根据需要调整
                    box(popup_window, 0 , 0); // 给悬浮窗口加边框
                    keypad(popup_window, TRUE); // 在创建窗口后立即调用
                }

                mvwprintw(popup_window, 1, 1, "Username:");
                mvwprintw(popup_window, 2, 1, "Password:");
                mvwprintw(popup_window, 4, 1, "Press again F1 to login.");

                // 创建表单字段
                FIELD* field[3];
                field[0] = new_field(1, 12, 0, 1, 0, 0); // 账号输入框
                field[1] = new_field(1, 12, 1, 1, 0, 0); // 密码输入框
                field[2] = NULL; // 字段数组的终止

                // 设置字段属性
                set_field_back(field[0], A_UNDERLINE);  // 为输入装饰下划线
                field_opts_off(field[0], O_AUTOSKIP);   // 不自动跳到下一个字段

                set_field_back(field[1], A_UNDERLINE);  // 同上
                //field_opts_off(field[1], O_PUBLIC);     // 密码不显示输入（非公开）
                field_opts_off(field[1], O_AUTOSKIP);

                // 创建表单
                FORM* form = new_form(field);
                set_form_win(form, popup_window);       // 将表单与悬浮窗口关联
                int rows, cols;
                scale_form(form, &rows, &cols);
                set_form_sub(form, derwin(popup_window, rows, cols, 1, 10)); // 设置表单子窗口以放置字段
                post_form(form);                        // 绘制表单
                wrefresh(popup_window);

                //form_driver(form, REQ_FIRST_FIELD);     // 将光标置于第一个字段


                int inner_ch;
                while ((inner_ch = getch()) != KEY_F(1)) { // 使用 wgetch 监听悬浮窗口
                    switch (inner_ch) {
                        case KEY_DOWN:
                            form_driver(form, REQ_NEXT_FIELD);
                            form_driver(form, REQ_END_LINE); // 跳到字段的末尾
                            break;
                        case KEY_UP:
                            //printf("UP\n");
                            form_driver(form, REQ_PREV_FIELD);
                            form_driver(form, REQ_END_LINE);
                            break;
                        case KEY_BACKSPACE:
                        case 127:  // 处理删除键
                            form_driver(form, REQ_DEL_PREV);
                            break;
                        default:
                            form_driver(form, inner_ch);
                            break;
                    }
                    wrefresh(popup_window);
                }
                form_driver(form, REQ_FIRST_FIELD);

                std::string username = trim_whitespaces(field_buffer(field[0], 0));
                std::string password = trim_whitespaces(field_buffer(field[1], 0));

                //char* username = trim_whitespaces(field_buffer(field[0], 0));
                //char* password = trim_whitespaces(field_buffer(field[1], 0));
                //std::cerr << password << "\n";
                if (login_and_save_token(username, password)) {
                    // 登录并保存令牌
                    // 关闭表单和窗口
                    unpost_form(form);
                    free_form(form);
                    for (int i = 0; field[i]; i++) {
                        free_field(field[i]);
                    }
                    delwin(popup_window);
                    popup_window = nullptr;
                    current_state = BLOG_VIEW; // 返回博客视图
                } else {
                    // 登录失败
                    mvwprintw(popup_window, 4, 1, "Login failed. Press F1 to close.");
                    wrefresh(popup_window);
                    while (getch() != KEY_F(1)); // 等待用户按下F1键
                    unpost_form(form);
                    free_form(form);
                    for (int i = 0; field[i]; i++) {
                        free_field(field[i]);
                    }
                    delwin(popup_window);
                    popup_window = nullptr;
                    current_state = BLOG_VIEW; // 返回博客视图
                }; // 登录并保存令牌
                /*
                // 关闭表单和窗口
                unpost_form(form);
                free_form(form);
                for (int i = 0; field[i]; i++) {
                    free_field(field[i]);
                }
                delwin(popup_window);
                popup_window = nullptr;
                current_state = BLOG_VIEW; // 返回博客视图
                 */
            }
            clear();
            refresh();
            break;


        case KEY_F(2):
            //check if token file exists
            if (current_state == BLOG_VIEW) {

                std::ifstream token_file("token");
                if (!token_file.is_open()) {
                    //if token file does not exist, show a message on popup window and press esc to close
                    if (!popup_window) {
                        popup_window = newwin(10, 50, 6, 10); // 创建新窗口，尺寸和位置可以根据需要调整
                        box(popup_window, 0, 0); // 给悬浮窗口加边框
                        mvwprintw(popup_window, 1, 1, "Please login first to create a post.");
                        mvwprintw(popup_window, 2, 1, "Press F2 to close.");
                        wrefresh(popup_window);
                    }
                    while (getch() != KEY_F(2)); // 等待用户按下F1键
                    delwin(popup_window);
                    popup_window = nullptr;
                    clear();
                    refresh();
                    break;
                }

                current_state = LOGIN_VIEW; // 切换到登录窗口状态

                // 创建或显示悬浮窗口
                if (!popup_window) {
                    popup_window = newwin(10, 30, 6, 10); // 创建新窗口，尺寸和位置可以根据需要调整
                    box(popup_window, 0, 0); // 给悬浮窗口加边框
                    keypad(popup_window, TRUE); // 在创建窗口后立即调用
                }

                mvwprintw(popup_window, 1, 1, "Title:");

                // 创建表单字段
                FIELD *field[2];
                field[0] = new_field(1, 12, 0, 1, 0, 0); // 标题输入框
                field[1] = NULL; // 字段数组的终止

                // 设置字段属性
                set_field_back(field[0], A_UNDERLINE);  // 为输入装饰下划线
                field_opts_off(field[0], O_AUTOSKIP);   // 不自动跳到下一个字段

                // 创建表单
                FORM *form = new_form(field);
                set_form_win(form, popup_window);       // 将表单与悬浮窗口关联
                int rows, cols;
                scale_form(form, &rows, &cols);
                set_form_sub(form, derwin(popup_window, rows, cols, 1, 10)); // 设置表单子窗口以放置字段
                post_form(form);                        // 绘制表单
                wrefresh(popup_window);

                int inner_ch;
                while ((inner_ch = getch()) != KEY_F(2)) { // 使用 wgetch 监听悬浮窗口
                    switch (inner_ch) {
                        case KEY_DOWN:
                            form_driver(form, REQ_NEXT_FIELD);
                            form_driver(form, REQ_END_LINE); // 跳到字段的末尾
                            break;
                        case KEY_UP:
                            //printf("UP\n");
                            form_driver(form, REQ_PREV_FIELD);
                            form_driver(form, REQ_END_LINE);
                            break;
                        case KEY_BACKSPACE:
                        case 127:  // 处理删除键
                            form_driver(form, REQ_DEL_PREV);
                            break;
                        default:
                            form_driver(form, inner_ch);
                            break;
                    }
                    wrefresh(popup_window);
                }
                form_driver(form, REQ_FIRST_FIELD);

                std::string title = trim_whitespaces(field_buffer(field[0], 0));
                create_post(title);

                // 关闭表单和窗口
                unpost_form(form);
                free_form(form);
                for (int i = 0; field[i]; i++) {
                    free_field(field[i]);
                }
                delwin(popup_window);
                popup_window = nullptr;
                current_state = BLOG_VIEW; // 返回博客视图
                clear();
                refresh();
            }
            break;


        case KEY_DOWN:
            offset++; // 向下滚动
            break;
        case KEY_UP:
            if (offset > 0) offset--; // 向上滚动
            break;
        case 'q':   // 按 'q' 退出
            endwin();
            exit(0);
        case KEY_NPAGE:
            if (sidebar_offset <= posts.size()-2) {
                sidebar_offset++; // 向下滚动侧边栏
            }else {
                sidebar_offset = 0;
            }
            index = (index + 1) % posts.size(); // 下一个帖子
            offset = 0; // 重置偏移量
            break;
        case KEY_PPAGE:
            if (sidebar_offset > 0) {
                sidebar_offset--; // 向上滚动侧边栏
            }else{
                sidebar_offset = posts.size()-1;
            }
            if (--index < 0) index = posts.size() - 1;
            offset = 0; // 重置偏移量
            break;
        case KEY_F(5):  // F5 键刷新
            posts = fetch_and_parse_posts();  // 重新获取文章列表
            if (posts.empty()) {
                printw("No posts available or failed to fetch posts.");
                refresh();
            }
            index = 0;  // 重置到第一个帖子
            offset = 0; // 重置滚动偏移量
            display_sidebar(posts, index,sidebar_offset, sidebar_win);  // 重新显示侧边栏
            display_post(posts[index], offset, content_win);  // 重新显示内容
            break;
    }
}


char* trim_whitespaces(char* str) {
    std::string s(str);

    // 删除前导空白
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));

    // 删除尾部空白
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());

    // 将std::string转换为char*
    char* trimmed = new char[s.length() + 1];
    std::strcpy(trimmed, s.c_str());

    return trimmed;
}

void display_sidebar(const std::vector<Post>& posts, int current_index, int offset, WINDOW* sidebar_win) {
    werase(sidebar_win);  // 清除侧边栏窗口
    int max_y, max_x;
    getmaxyx(sidebar_win, max_y, max_x); // 获取侧边栏窗口的大小
    for (int i = 0; i < max_y && i + offset < posts.size(); ++i) {
        if (i + offset == current_index) {
            wattron(sidebar_win, A_REVERSE);  // 开启反向高亮
        }
        std::string title = posts[i + offset].title;
        if (title.size() > 20) {
            title = title.substr(0, 20) + "...";
        }
        mvwprintw(sidebar_win, i, 0, "%s", title.c_str());
        if (i + offset == current_index) {
            wattroff(sidebar_win, A_REVERSE);  // 关闭反向高亮
        }
    }
    wrefresh(sidebar_win);  // 刷新侧边栏窗口以显示更新的内容
}


void display_posts(const std::vector<Post>& posts) {
    int index = 0;
    int offset = 0;
    int sidebar_offset = 0;
    WINDOW* sidebar_win = newwin(getmaxy(stdscr), 23, 0, 0);  // 创建侧边栏窗口，宽度为20
    WINDOW* content_win = newwin(getmaxy(stdscr), getmaxx(stdscr) - 25, 0, 25);  // 创建内容窗口

    while (true) {
        if (current_state == BLOG_VIEW) {
            display_sidebar(posts, index,sidebar_offset, sidebar_win);
            display_post(posts[index], offset, content_win);
            // 绘制竖线
            for (int y = 0; y < getmaxy(stdscr); y++) {
                //mvwaddch(stdscr, y, 23, '|');  // 在第20列绘制 '|'
                mvwaddch(stdscr, y, 23, ACS_VLINE);  // 使用 ncurses 的图形字符绘制线

            }
        }
        handle_user_input(const_cast<std::vector<Post>&>(posts), index, offset,sidebar_offset , sidebar_win, content_win);
        //handle_view_change_input(); // 处理视图切换输入
        refresh();  // 刷新整个屏幕，包括侧边栏和内容窗口
    }

    delwin(sidebar_win);
    delwin(content_win);
}


int main() {
    // 假设 fetch_and_parse_posts 已经定义并返回一个 vector<Post>
    std::vector<Post> posts = fetch_and_parse_posts();

    init_ncurses();     // 初始化 ncurses

    display_posts(posts); // 显示帖子并处理滚动

    endwin();           // 结束 ncurses 模式

    return 0;
}
