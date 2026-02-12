/*
 * Copyright (C) 2026 Gabriel Paes <gabriel.paesbarreto@ufrpe.br>
 * * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Widgets Globais */
GtkWidget *entry_valor;
GtkWidget *combo_de;
GtkWidget *combo_para;
GtkWidget *label_resultado;
GtkWidget *switch_lang;
GtkWidget *btn_converter; /* Promovido a global para poder mudar o texto */

GtkListStore *store_pt;
GtkListStore *store_en;

int idioma = 0;

struct Memory {
    char *data;
    size_t size;
};

typedef struct {
    char code[4];
    double rate;
} Rate;

Rate rates[5] = {
    {"USD", 1.0},
    {"BRL", 0},
    {"GBP", 0},
    {"EUR", 0},
    {"CAD", 0}
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct Memory *mem = userp;

    mem->data = realloc(mem->data, mem->size + realsize + 1);
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

void atualizar_rates() {
    CURL *curl = curl_easy_init();
    struct Memory chunk = {0};

    curl_easy_setopt(curl, CURLOPT_URL,
        "https://api.frankfurter.app/latest?from=USD&to=USD,BRL,GBP,EUR,CAD");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    for (int i = 0; i < 5; i++) {
        char busca[16];
        sprintf(busca, "\"%s\":", rates[i].code);
        char *pos = strstr(chunk.data, busca);
        if (pos) {
            char *start = pos + strlen(busca);
            rates[i].rate = g_ascii_strtod(start, NULL);
        }
    }

    free(chunk.data);
}

gpointer atualizar_rates_thread(gpointer data) {
    atualizar_rates();
    return NULL;
}

gboolean timer_update(gpointer data) {
    g_thread_new("rates", atualizar_rates_thread, NULL);
    return TRUE;
}

GtkListStore* criar_store(const char *lang) {
    GtkListStore *store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    GtkTreeIter iter;

    struct {
        const char *flag;
        const char *pt;
        const char *en;
        const char *code;
    } moedas[] = {
        {"/yacc/flags/br.png", "Real brasileiro", "Brazilian Real", "BRL"},
        {"/yacc/flags/us.png", "Dólar americano", "US Dollar", "USD"},
        {"/yacc/flags/gb.png", "Libra esterlina", "British Pound", "GBP"},
        {"/yacc/flags/eu.png", "Euro", "Euro", "EUR"},
        {"/yacc/flags/ca.png", "Dólar canadense", "Canadian Dollar", "CAD"}
    };

    for (int i = 0; i < 5; i++) {
        GdkPixbuf *pix = gdk_pixbuf_new_from_resource_at_scale(
            moedas[i].flag, 24, 16, TRUE, NULL);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0, pix,
            1, strcmp(lang, "pt") == 0 ? moedas[i].pt : moedas[i].en,
            2, moedas[i].code,
            -1);
    }

    return store;
}

const char* get_code(GtkComboBox *combo) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    gtk_combo_box_get_active_iter(combo, &iter);
    model = gtk_combo_box_get_model(combo);

    gchar *code;
    gtk_tree_model_get(model, &iter, 2, &code, -1);
    return code;
}

double get_rate(const char *code) {
    for (int i = 0; i < 5; i++)
        if (strcmp(code, rates[i].code) == 0)
            return rates[i].rate;
    return 0;
}

void converter(GtkButton *button, gpointer data) {
    double valor = g_ascii_strtod(gtk_entry_get_text(GTK_ENTRY(entry_valor)), NULL);

    const char *de = get_code(GTK_COMBO_BOX(combo_de));
    const char *para = get_code(GTK_COMBO_BOX(combo_para));

    double r_de = get_rate(de);
    double r_para = get_rate(para);

    if (r_de == 0 || r_para == 0) {
        gtk_label_set_text(GTK_LABEL(label_resultado), 
            idioma ? "Rates not loaded" : "Taxas não carregadas");
        return;
    }

    double resultado = valor * (r_para / r_de);

    char texto[64];
    sprintf(texto, "%.2f %s", resultado, para);
    gtk_label_set_text(GTK_LABEL(label_resultado), texto);
}

gboolean trocar_idioma(GtkSwitch *sw, gboolean state, gpointer data) {
    idioma = state;

    /* Atualiza as listas dos comboboxes */
    gtk_combo_box_set_model(GTK_COMBO_BOX(combo_de),
        idioma ? GTK_TREE_MODEL(store_en) : GTK_TREE_MODEL(store_pt));
    gtk_combo_box_set_model(GTK_COMBO_BOX(combo_para),
        idioma ? GTK_TREE_MODEL(store_en) : GTK_TREE_MODEL(store_pt));

    /* Atualiza os textos estáticos */
    if (idioma) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry_valor), "Amount");
        gtk_button_set_label(GTK_BUTTON(btn_converter), "Convert");
    } else {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry_valor), "Valor");
        gtk_button_set_label(GTK_BUTTON(btn_converter), "Converter");
    }

    /* Reseta a seleção */
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_de), 1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_para), 0);

    return FALSE;
}

GtkWidget* criar_combo(GtkListStore *store) {
    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

    GtkCellRenderer *pix = gtk_cell_renderer_pixbuf_new();
    GtkCellRenderer *txt = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), pix, FALSE);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), txt, TRUE);

    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), pix, "pixbuf", 0, NULL);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), txt, "text", 1, NULL);

    return combo;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    atualizar_rates();
    g_timeout_add_seconds(600, timer_update, NULL);

    store_pt = criar_store("pt");
    store_en = criar_store("en");

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "YACC — Yet Another Currency Converter");
    gtk_window_set_default_size(GTK_WINDOW(win), 420, 340);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(win), box);
    gtk_container_set_border_width(GTK_CONTAINER(box), 15);

    /* BOX DO SWITCH COM BANDEIRAS */
    GtkWidget *lang_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    GtkWidget *img_br = gtk_image_new_from_resource("/yacc/flags/br.png");
    GtkWidget *img_us = gtk_image_new_from_resource("/yacc/flags/us.png");

    switch_lang = gtk_switch_new();

    gtk_box_pack_start(GTK_BOX(lang_box), img_br, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lang_box), switch_lang, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(lang_box), img_us, FALSE, FALSE, 0);

    gtk_widget_set_halign(lang_box, GTK_ALIGN_START);

    entry_valor = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_valor), "Valor");

    combo_de = criar_combo(store_pt);
    combo_para = criar_combo(store_pt);

    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_de), 1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_para), 0);


    btn_converter = gtk_button_new_with_label("Converter");
    
    label_resultado = gtk_label_new("---");

    GtkWidget *label_creditos = gtk_label_new("Copyright © 2026 Gabriel Paes — v1.0.1");
    gtk_widget_set_halign(label_creditos, GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(box), lang_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry_valor, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo_de, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo_para, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), btn_converter, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label_resultado, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(box), label_creditos, FALSE, FALSE, 0);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/yacc/style.css");
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_USER);

    /* Conexão de sinais atualizada para usar btn_converter */
    g_signal_connect(btn_converter, "clicked", G_CALLBACK(converter), NULL);
    g_signal_connect(switch_lang, "state-set", G_CALLBACK(trocar_idioma), NULL);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
