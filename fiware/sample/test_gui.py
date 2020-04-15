import urllib.request
from tkinter import *
import json

# Common Variables
url_entry_textbox_width = 75

class MyFirstGUI(Tk):

    def __init__(self):
        # create main window by calling the __init__ method of parent class Tk
        Tk.__init__(self)
        self.geometry("640x480")
        self.title("Robotics Use Case Console")

        label1 = Label(self, text="Prepare your HTTP request")
        label1.pack()

        ##Give a default, customisable DOI value
        label2 = Label(self, text="url:")
        label2.pack()
        self.entry1 = Entry(self, bd=5, width=url_entry_textbox_width)
        self.entry1.insert(0, 'http://localhost:1026/v2/entities/my_robot')
        self.entry1.pack()

        submit = Button(self, text ="Submit", command = lambda: self.update("get"))
        submit.pack()

        close_button = Button(self, text="Close", command=self.quit)
        close_button.pack()

        ##Here I want to produce the result of my http request call
        self.w = Text(self, relief='flat', 
                      bg = self.cget('bg'),
                      highlightthickness=0, height=100) 
        # trick to make disabled text copy/pastable
        self.w.bind("<1>", lambda event: self.w.focus_set())
        self.w.insert('1.0', "Output area for result of HTTP request to be updated when you press submit\n"
                                        +"(ideally highlightable/copy pastable)")
        self.w.configure(state="disabled", inactiveselectbackground=self.w.cget("selectbackground"))
        self.w.pack()

        self.mainloop()

    def update_text(self, new_text):
        """ update the content of the text widget """
        self.w.configure(state='normal')
        self.w.delete('1.0', 'end')    # clear text
        # Decode UTF-8 bytes to Unicode, and convert single quotes 
        # to double quotes to make it valid JSON
        my_json = new_text.decode('utf8').replace("'", '"')
        # Load the JSON to a Python list & dump it back out as formatted JSON
        data = json.loads(my_json)
        s = json.dumps(data, indent=4, sort_keys=True)
        self.w.insert('1.0', s) # display new text
        self.w.configure(state='disabled') 


    def update(self, method):
        if method == "get":
          doi = str(self.entry1.get()) ##Get the user inputted DOI 
          print(str(self.entry1.get()))
          url = doi
          print(url)

        try:
            x = urllib.request.urlopen(url)
        except urllib.error.URLError as e: 
            ##Show user an error if they put in the wrong DOI
            self.update_text(str(e)) 

        else:
            ##Update the output area to the returned form of the text entry, ideally highlightable for copying
            data = x.read()
            self.update_text(data) 

if __name__ == '__main__':
    my_gui = MyFirstGUI()
