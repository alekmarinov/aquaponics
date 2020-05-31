import os
import csv
from datetime import datetime
import flask
import dash
import dash_core_components as dcc
import dash_html_components as html
from dash.dependencies import Input, Output

ESP32_WROOM_32_CSV = os.environ['ESP32_WROOM_32_CSV']
static_directory = 'static'

def load_data(filename):
    with open(filename) as csvfile:
        reader = csv.DictReader(csvfile, delimiter=',')
        prev_date_time = None
        x_axis = []
        y_axis_map = {}
        for row in reader:
            when = datetime.strptime(row['when'], '%Y-%m-%d %H:%M:%S')
            x_axis.append(when)
            for key, value in row.items():
                if key != 'when':
                    if not key in y_axis_map:
                        y_axis_map[key] = []
                    y_axis_map[key].append(float(value))
        return x_axis, y_axis_map

data = load_data(ESP32_WROOM_32_CSV)

external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']

app = dash.Dash(__name__, external_stylesheets=external_stylesheets)

@app.server.route(f'/static/<file_path>')
def serve_image(file_path):
    return flask.send_from_directory(static_directory, file_path)

app.title = 'Aquaponics Station'

app.layout = html.Div([
    # dcc.Input(id='my-id', value='initial value', type='text'),
    # html.Div(id='my-div'),
    html.H1(style={'text-align': 'center'}, children=["Aquaponics Station"]),
    html.Div(style={'display': 'flex', 'margin': 'auto', 'width': '90%'}, children=[
        html.Div([
            html.P('Camera 1'),
            html.Img(src='/static/fake_cam_plants.jpg', style={'padding': '10px'})
        ]),
        html.Div([
            html.P('Camera 2'),
            html.Img(src='/static/fake_cam_fish.jpg', style={'padding': '10px'})
        ])
    ]),
    dcc.Graph(
      id='temperatures',
      figure={
         'data': [
            {'x': data[0], 'y': data[1]['water_temperature'], 'type': 'line', 'name': 'Water temperature'},
            {'x': data[0], 'y': data[1]['air_temperature'], 'type': 'line', 'name': 'Air temperature'},
         ],
         'layout': {
            'title': 'Temperatures'
         }
      }
    ),
    dcc.Graph(
      id='moisture',
      figure={
         'data': [
            {'x': data[0], 'y': data[1]['moisture'], 'type': 'line', 'name': 'Moisture'}
         ],
         'layout': {
            'title': 'Moisture'
         }
      }
    ),
    dcc.Graph(
      id='pH',
      figure={
         'data': [
            {'x': data[0], 'y': data[1]['ph'], 'type': 'line', 'name': 'pH'}
         ],
         'layout': {
            'title': 'pH'
         }
      }
    )
])


# @app.callback(
#     Output(component_id='my-div', component_property='children'),
#     [Input(component_id='my-id', component_property='value')]
# )
# def update_output_div(input_value):
#     return html.Div(id='another-div', 
#         style={'backgroundColor': '#aaaa22'},
#         children=[
#         'You\'ve entered "{}"'.format(input_value)
#     ])


if __name__ == '__main__':
    app.run_server(debug=False, port=80, host='0.0.0.0')
