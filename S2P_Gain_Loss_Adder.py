import skrf as rf
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import os

def apply_gain_or_loss_to_s2p(input_file, output_file, gain_db):
    """
    Function to read an s2p file, applies a fixed gain (positive dB) or loss (negative dB)
    to all S-parameters, and writes the modified data to a new s2p file.
    """
    try:
        network = rf.Network(input_file)
        
        # Convert dB value (positive for gain, negative for loss) to a linear magnitude factor
        # Formula: Factor = 10^(dB_value / 20)
        magnitude_factor = 10**(gain_db / 20.0)

        # Apply the factor
        network.s = network.s * magnitude_factor

        # Save the modified network
        network.write_touchstone(output_file)
        return True
    except Exception as e:
        messagebox.showerror("Error", f"An error occurred while processing {input_file}: {e}")
        return False

def process_files(gain_entry_widget, status_label, root_window):
    """
    Handles GUI interaction, input validation, file selection,
    and calls the processing function for each file.
    Accepts the root window object to close it later.
    """
    status_label.config(text="Status: Starting process...")
    
    # 1. Validate the input gain value
    try:
        gain_db = float(gain_entry_widget.get())
    except ValueError:
        messagebox.showerror("Invalid Input", "Please enter a valid numeric value (e.g., 5.0 for gain, -0.2 for loss).")
        status_label.config(text="Status: Idle")
        return

    # Determine the suffix for the filename based on gain/loss value
    gain_loss_type = "gain" if gain_db >= 0 else "loss"
    gain_db_str = f"{gain_db:g}".replace('.', 'p').replace('-', 'minus')
    filename_suffix = f"_modified_{gain_db_str}dB_{gain_loss_type}"
    
    # 2. Open file dialog to select multiple .s2p files
    file_paths = filedialog.askopenfilenames(
        title="Select S2P Files to Process",
        filetypes=[("S2P files", "*.s2p"), ("All files", "*.*")]
    )

    if not file_paths:
        status_label.config(text="Status: No files selected. Idle.")
        return

    status_label.config(text=f"Status: Processing {len(file_paths)} files with {gain_db} dB {gain_loss_type}...")
    output_folder_name = 'modified_files'
    files_processed_count = 0

    for input_path in file_paths:
        input_directory = os.path.dirname(input_path)
        filename_base, file_extension = os.path.splitext(os.path.basename(input_path))
        new_filename = f"{filename_base}{filename_suffix}{file_extension}"
        output_directory = os.path.join(input_directory, output_folder_name)

        try:
            os.makedirs(output_directory, exist_ok=True)
        except OSError as e:
            messagebox.showerror("Directory Error", f"Error creating directory {output_directory}: {e}")
            continue

        output_path = os.path.join(output_directory, new_filename)
        
        if apply_gain_or_loss_to_s2p(input_path, output_path, gain_db):
            files_processed_count += 1
        
    status_label.config(text=f"Status: Complete! Processed {files_processed_count} files.")
    
    # Show completion message
    messagebox.showinfo("Success", f"Finished processing {files_processed_count} files.\nSaved to a '{output_folder_name}' folder with detailed names.")
    
    # Close the GUI window after the user clicks 'OK' on the info box
    root_window.destroy()


def create_gui():
    """
    Sets up the main Tkinter application window.
    """
    root = tk.Tk()
    root.title("S2P Gain/Loss Applier")
    root.geometry("400x160")
    
    style = ttk.Style()
    style.configure("TLabel", padding=5)
    style.configure("TButton", padding=5)

    frame = ttk.Frame(root, padding="10 10 10 10")
    frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
    
    # Label and Entry for gain/loss value
    ttk.Label(frame, text="Gain/Loss Value (dB):").grid(column=1, row=1, sticky=tk.W)
    gain_entry = ttk.Entry(frame, width=10)
    gain_entry.grid(column=2, row=1, sticky=(tk.W, tk.E))
    gain_entry.insert(0, "0.2")

    # Status Label
    status_var = tk.StringVar(value="Status: Idle")
    status_label = ttk.Label(frame, textvariable=status_var, wraplength=350)
    status_label.grid(column=1, row=3, columnspan=2, pady=(10, 0))

    # Button to trigger the process
    # Pass the 'root' object to the process_files function so it can be destroyed
    process_button = ttk.Button(
        frame, 
        text="Select Files and Process", 
        command=lambda: process_files(gain_entry, status_label, root)
    )
    process_button.grid(column=1, row=2, columnspan=2, pady=10)

    # Configure the grid to resize nicely
    for child in frame.winfo_children(): 
        child.grid_configure(padx=5, pady=5)

    root.mainloop()

# --- Main Execution ---
if __name__ == "__main__":
    # Ensure necessary libraries are installed before running:
    # pip install scikit-rf numpy
    create_gui()
