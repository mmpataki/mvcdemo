#include "mGLapi.h"
#include "thelper.h"

#define VIEW_TYPE	DotChart
#define INIT_PAGE	"dot.vsml"

char *user_name = "Dot chart user";
TextBlock *message_block;
VIEW_TYPE *view;
ChartData *graph_data;

void add_clicked();
void commit_clicked();
void connect_clicked();
void view_selection_changed(int index);
void set_message(char *message);


/*
 * include the protofunctions.h here so that
 * required variables are already got declared.
 */
#include "protofunctions.h"

void add_clicked() {
	add_new_item();
}

void connect_clicked() {
	tcreate(connect_to_model, graph_data);
}

void set_message(char *message)
{
	message_block->setText(message);
}

void commit_clicked()
{
	send_update(view->getSelectedItemIndex());
}

void view_selection_changed(int index)
{
	if (index == -1) {
		/* nothing changed */
		return;
	}

	char buffer[100];
	sprintf(buffer, "Items selection changed. [index: %d]", index + 1);
	set_message(buffer);
}

void register_events()
{
	REGISTER("Adder", Button, mouse_clicked, add_clicked);
	REGISTER("Commit", Button, mouse_clicked, commit_clicked);
	REGISTER("Connect", Button, mouse_clicked, connect_clicked);
	REGISTER("View", VIEW_TYPE, selection_changed, view_selection_changed);

	view = (VIEW_TYPE*)GET_ELEMENT("View");
	message_block = (TextBlock*)GET_ELEMENT("MessageBox");

	if (view) {
		view->setDataSource(graph_data);
	}
}

void app_started()
{
	((Frame*)main_frame)->Navigate(INIT_PAGE);
	graph_data = new ChartData(15);
	connect_clicked();
}
