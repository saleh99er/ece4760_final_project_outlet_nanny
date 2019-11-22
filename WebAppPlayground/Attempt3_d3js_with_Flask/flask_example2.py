from flask import Flask, render_template
import datetime
#set the project root directory as the static folder, you can set others.
app = Flask(__name__, static_url_path='')

@app.route('/')
def send_index(path):
	print(datetime.datetime.now())
	return render_template('index.html', message=message)
	
if __name__ == "__main__":
	app.run(debug=True)
